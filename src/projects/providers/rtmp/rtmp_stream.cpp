//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"

#include <base/info/media_extradata.h>
#include <base/mediarouter/media_type.h>
#include <modules/bitstream/aac/audio_specific_config.h>
#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/containers/flv/flv_parser.h>
#include <modules/id3v2/frames/id3v2_frames.h>
#include <modules/id3v2/id3v2.h>
#include <orchestrator/orchestrator.h>

#include "base/info/application.h"
#include "base/provider/push_provider/application.h"
#include "base/provider/push_provider/provider.h"
#include "rtmp_provider_private.h"

/*
Process of publishing 

- Handshaking
 C->S : C0 + C1
 S->C : S0 + S1 + S2
 C->S : C2

- Connect
 C->S : Set Chunk Size
 C->S : Connect
 S->C : WindowAcknowledgement Size
 S->C : Set Peer Bandwidth
 C->S : Window Acknowledgement Size
 S->C : Stream Begin
 S->C : Set Chunk Size
 S->C : _result

- Publishing
 C->S : releaseStream
 C->S : FCPublish
 C->S : createStream
 S->C : _result
 C->S : publish

 S->C : Stream Begin
 S->C : onStatus(Start)
 C->S : @setDataFrame
 C->S : Video/Audio Data Stream
 - H.264 : SPS/PPS 
 - AAC : Control Byte 
*/

// When using b-frames in Wirecast, sometimes CTS is negative, so a PTS < DTS situation occurs.
// This is not a normal situation, so we need to adjust it.
//
// [OBS]
// DTS   CTS   PTS
//   0     0     0
//   0    66    66
//  33   166   199
//  66    66   132
//  99     0    99
// 132    33   165
//
// [Wirecast]
// DTS   CTS   PTS
//   0     0     0
//  33   100   133
//  66     0    66
// 100   -66    34 <== ERROR (PTS < DTS)
// 133   -33   100 <== ERROR (PTS < DTS)
// 166   100   266
// 200     0   200
#define ADJUST_PTS (150)

namespace pvd
{
	std::shared_ptr<RtmpStream> RtmpStream::Create(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<ov::Socket> &client_socket, const std::shared_ptr<PushProvider> &provider)
	{
		auto stream = std::make_shared<RtmpStream>(source_type, client_id, client_socket, provider);
		if (stream != nullptr)
		{
			stream->Start();
		}
		return stream;
	}

	RtmpStream::RtmpStream(StreamSourceType source_type, uint32_t client_id, std::shared_ptr<ov::Socket> client_socket, const std::shared_ptr<PushProvider> &provider)
		: PushStream(source_type, client_id, provider),

		  _vhost_app_name(info::VHostAppName::InvalidVHostAppName())
	{
		logtd("Stream has been created");

		_remote = client_socket;
		SetMediaSource(_remote->GetRemoteAddressAsUrl());

		_import_chunk = std::make_shared<RtmpChunkParser>(RTMP_DEFAULT_CHUNK_SIZE);
		_export_chunk = std::make_shared<RtmpExportChunk>(false, RTMP_DEFAULT_CHUNK_SIZE);
		_media_info = std::make_shared<RtmpMediaInfo>();

		// For debug statistics
		_stream_check_time = time(nullptr);
	}

	RtmpStream::~RtmpStream()
	{
		logtd("Stream has been terminated finally");
	}

	bool RtmpStream::Start()
	{
		SetState(Stream::State::PLAYING);
		_negative_cts_detected = false;

		return PushStream::Start();
	}

	bool RtmpStream::Stop()
	{
		if (GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		// Send Close to Admission Webhooks
		auto requested_url = GetRequestedUrl();
		auto final_url = GetFinalUrl();
		if (_remote && requested_url && final_url)
		{
			auto remote_address{_remote->GetRemoteAddress()};
			if (remote_address)
			{
				auto request_info = std::make_shared<AccessController::RequestInfo>(requested_url, remote_address, requested_url->ToUrlString(true) == final_url->ToUrlString(true) ? nullptr : final_url);

				GetProvider()->SendCloseAdmissionWebhooks(request_info);
			}
		}
		// the return check is not necessary

		if (_remote->GetState() == ov::SocketState::Connected)
		{
			_remote->Close();
		}

		return PushStream::Stop();
	}

	bool RtmpStream::CheckStreamExpired()
	{
		if (_stream_expired_msec != 0 && _stream_expired_msec < ov::Clock::NowMSec())
		{
			return true;
		}

		return false;
	}

	bool RtmpStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		if (GetState() == Stream::State::ERROR || GetState() == Stream::State::STOPPED)
		{
			return false;
		}

		// Check stream expired by signed policy
		if (CheckStreamExpired() == true)
		{
			logti("Stream has expired by signed policy (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
			Stop();
			return false;
		}

		// Accumulate processed bytes for acknowledgement
		auto data_length = data->GetLength();
		_acknowledgement_traffic += data_length;
		if (_acknowledgement_traffic >= INT_MAX)
		{
			// Rolled
			_acknowledgement_traffic -= INT_MAX;
		}
		_acknowledgement_traffic_after_last_acked += data_length;

		if ((_remained_data == nullptr) || _remained_data->IsEmpty())
		{
			_remained_data = data->Clone();
		}
		else
		{
			_remained_data->Append(data);
		}

		if (_remained_data->GetLength() > RTMP_MAX_PACKET_SIZE)
		{
			logte("The packet is ignored because the size is too large: [%d]), packet size: %zu, threshold: %d",
				  GetChannelId(), _remained_data->GetLength(), RTMP_MAX_PACKET_SIZE);

			return false;
		}

		logtp("Trying to parse data\n%s", _remained_data->Dump(_remained_data->GetLength()).CStr());

		while (true)
		{
			int32_t process_size = 0;

			if (_handshake_state == RtmpHandshakeState::Complete)
			{
				process_size = ReceiveChunkPacket(_remained_data);
			}
			else
			{
				process_size = ReceiveHandshakePacket(_remained_data);
			}

			if (process_size < 0)
			{
				logtd("Could not parse RTMP packet: [%s/%s] (%u/%u), size: %zu bytes, returns: %d",
					  _vhost_app_name.CStr(), _stream_name.CStr(),
					  _app_id, GetId(),
					  _remained_data->GetLength(),
					  process_size);

				return process_size;
			}
			else if (process_size == 0)
			{
				// Need more data
				// logtd("Not enough data");
				break;
			}

			_remained_data = _remained_data->Subdata(process_size);
		}

		if (_acknowledgement_traffic_after_last_acked > _acknowledgement_size)
		{
			SendAcknowledgementSize(_acknowledgement_traffic);
			_acknowledgement_traffic_after_last_acked -= _acknowledgement_size;
		}

		return true;
	}

	void RtmpStream::OnAmfConnect(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		double object_encoding = 0.0;

		if (document.GetProperty(2) != nullptr && document.GetProperty(2)->GetType() == AmfDataType::Object)
		{
			AmfObject *object = document.GetProperty(2)->GetObject();
			int32_t index;

			// object encoding
			if ((index = object->FindName("objectEncoding")) >= 0 && object->GetType(index) == AmfDataType::Number)
			{
				object_encoding = object->GetNumber(index);
			}

			// app name set
			if ((index = object->FindName("app")) >= 0 && object->GetType(index) == AmfDataType::String)
			{
				_app_name = object->GetString(index);
			}

			// app url set
			if ((index = object->FindName("tcUrl")) >= 0 && object->GetType(index) == AmfDataType::String)
			{
				_tc_url = object->GetString(index);
			}
		}

		// Parse the URL to obtain the domain name
		{
			_url = ov::Url::Parse(_tc_url);
			if (_url != nullptr)
			{
				_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(_url->Host(), _app_name);
				_import_chunk->SetAppName(_vhost_app_name);

				//Since vhost/app/stream can be changed in AdmissionWebhooks, it is not checked here.
				// auto app_info = ocst::Orchestrator::GetInstance()->GetApplicationInfo(_vhost_app_name);
				// if (app_info.IsValid())
				// {
				// 	_app_id = app_info.GetId();
				// }
				// else
				// {
				// 	logte("%s application does not exist", _vhost_app_name.CStr());
				// 	Stop();
				// 	return;
				// }
			}
			else
			{
				logtw("Could not obtain tcUrl from the RTMP stream: [%s]", _app_name.CStr());

				// TODO(dimiden): If tcUrl is not provided, it's not possible to determine which VHost the request was received,
				// so it does not work properly.
				// So, if there is currently one VHost associated with the RTMP Provider, we need to modify it to work without tcUrl.
				_vhost_app_name = info::VHostAppName("", _app_name);
				_import_chunk->SetAppName(_vhost_app_name);
			}
		}

		if (!SendWindowAcknowledgementSize(RTMP_DEFAULT_ACKNOWNLEDGEMENT_SIZE))
		{
			logte("SendWindowAcknowledgementSize Fail");
			return;
		}

		if (!SendSetPeerBandwidth(_peer_bandwidth))
		{
			logte("SendSetPeerBandwidth Fail");
			return;
		}

		if (!SendStreamBegin(0))
		{
			logte("SendStreamBegin Fail");
			return;
		}

		if (!SendAmfConnectResult(header->basic_header.chunk_stream_id, transaction_id, object_encoding))
		{
			logte("SendAmfConnectResult Fail");
			return;
		}
	}

	void RtmpStream::OnAmfCreateStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (!SendAmfCreateStreamResult(header->basic_header.chunk_stream_id, transaction_id))
		{
			logte("SendAmfCreateStreamResult Fail");
			return;
		}
	}

	bool RtmpStream::CheckAccessControl()
	{
		// Check SignedPolicy

		auto [result, _signed_policy] = GetProvider()->VerifyBySignedPolicy(_url, _remote->GetRemoteAddress());
		if (result == AccessController::VerificationResult::Off)
		{
		}
		else if (result == AccessController::VerificationResult::Pass)
		{
			_stream_expired_msec = _signed_policy->GetStreamExpireEpochMSec();
		}
		else if (result == AccessController::VerificationResult::Error)
		{
			logtw("SingedPolicy error : %s", _url->ToUrlString().CStr());
			Stop();
			return false;
		}
		else if (result == AccessController::VerificationResult::Fail)
		{
			logtw("%s", _signed_policy->GetErrMessage().CStr());
			Stop();
			return false;
		}

		auto request_info = std::make_shared<AccessController::RequestInfo>(_url, _remote->GetRemoteAddress());

		auto [webhooks_result, _admission_webhooks] = GetProvider()->VerifyByAdmissionWebhooks(request_info);
		if (webhooks_result == AccessController::VerificationResult::Off)
		{
			return true;
		}
		else if (webhooks_result == AccessController::VerificationResult::Pass)
		{
			// Lifetime
			if (_admission_webhooks->GetLifetime() != 0)
			{
				// Choice smaller value
				auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + _admission_webhooks->GetLifetime();
				if (_stream_expired_msec == 0 || stream_expired_msec_from_webhooks < _stream_expired_msec)
				{
					_stream_expired_msec = stream_expired_msec_from_webhooks;
				}
			}

			// Redirect URL
			if (_admission_webhooks->GetNewURL() != nullptr)
			{
				_publish_url = _admission_webhooks->GetNewURL();
				SetFinalUrl(_publish_url);
			}

			return true;
		}
		else if (webhooks_result == AccessController::VerificationResult::Error)
		{
			logtw("AdmissionWebhooks error : %s", _url->ToUrlString().CStr());
			Stop();
			return false;
		}
		else if (webhooks_result == AccessController::VerificationResult::Fail)
		{
			logtw("AdmissionWebhooks error : %s", _admission_webhooks->GetErrReason().CStr());
			Stop();
			return false;
		}

		return false;
	}

	bool RtmpStream::ValidatePublishUrl()
	{
		if (_publish_url == nullptr)
		{
			logte("Publish URL is not set");
			return false;
		}

		if ((_publish_url->Scheme().UpperCaseString() != "RTMP" && _publish_url->Scheme().UpperCaseString() != "RTMPS") ||
			_publish_url->Host().IsEmpty() ||
			_publish_url->App().IsEmpty() ||
			_publish_url->Stream().IsEmpty())
		{
			logte("Invalid publish URL: %s", _publish_url->ToUrlString().CStr());
			return false;
		}

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(_publish_url->Host(), _publish_url->App());

		auto app_info = ocst::Orchestrator::GetInstance()->GetApplicationInfo(vhost_app_name);
		if (app_info.IsValid())
		{
			_app_id = app_info.GetId();
		}
		else
		{
			logte("Could not find application: %s", vhost_app_name.CStr());
			return false;
		}

		return true;
	}

	void RtmpStream::OnAmfFCPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty() && document.GetProperty(3) != nullptr &&
			document.GetProperty(3)->GetType() == AmfDataType::String)
		{
			// TODO: check if the chunk stream id is already exist, and generates new rtmp_stream_id and client_id.
			if (!SendAmfOnFCPublish(header->basic_header.chunk_stream_id, _rtmp_stream_id, _client_id))
			{
				logte("SendAmfOnFCPublish Fail");
				return;
			}

			_full_url.Format("%s/%s", _tc_url.CStr(), document.GetProperty(3)->GetString());
			SetFullUrl(_full_url);
			CheckAccessControl();

			if (ValidatePublishUrl() == false)
			{
				Stop();
				return;
			}
		}
	}

	void RtmpStream::OnAmfPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty())
		{
			if (document.GetProperty(3) != nullptr && document.GetProperty(3)->GetType() == AmfDataType::String)
			{
				_full_url.Format("%s/%s", _tc_url.CStr(), document.GetProperty(3)->GetString());
				SetFullUrl(_full_url);
				CheckAccessControl();

				if (ValidatePublishUrl() == false)
				{
					Stop();
					return;
				}
			}
			else
			{
				logte("OnPublish - Publish Name None");

				//Reject
				SendAmfOnStatus(header->basic_header.chunk_stream_id,
								_rtmp_stream_id,
								(char *)"error",
								(char *)"NetStream.Publish.Rejected",
								(char *)"Authentication Failed.", _client_id);

				return;
			}
		}

		_chunk_stream_id = header->basic_header.chunk_stream_id;

		// stream begin 전송
		if (!SendStreamBegin(_rtmp_stream_id))
		{
			logte("SendStreamBegin Fail");
			return;
		}

		// 시작 상태 값 전송
		if (!SendAmfOnStatus((uint32_t)_chunk_stream_id,
							 _rtmp_stream_id,
							 (char *)"status",
							 (char *)"NetStream.Publish.Start",
							 (char *)"Publishing",
							 _client_id))
		{
			logte("SendAmfOnStatus Fail");
			return;
		}
	}

	bool RtmpStream::SetFullUrl(ov::String url)
	{
		_url = ov::Url::Parse(url);
		// PORT can be omitted (1935), but SignedPolicy requires this information.
		if (_url->Port() == 0)
		{
			_url->SetPort(_remote->GetLocalAddress()->Port());
		}

		_publish_url = _url;
		_stream_name = _url->Stream();
		_import_chunk->SetStreamName(_stream_name);

		SetRequestedUrl(_url);
		SetFinalUrl(_url);

		return true;
	}

	bool RtmpStream::OnAmfMetaData(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, int32_t object_index)
	{
		RtmpCodecType video_codec_type = RtmpCodecType::Unknown;
		RtmpCodecType audio_codec_type = RtmpCodecType::Unknown;
		bool video_Available = false;
		bool audio_Available = false;
		double frame_rate = 30.0;
		double video_width = 0;
		double video_height = 0;
		double video_bitrate = 0;
		double audio_bitrate = 0.0;
		double audio_channels = 1.0;
		double audio_samplerate = 0.0;
		double audio_samplesize = 0.0;
		AmfObjectArray *object = nullptr;
		int32_t index = 0;
		ov::String bitrate_string;
		RtmpEncoderType encoder_type = RtmpEncoderType::Custom;

		/*
		// dump 정보 출력
		std::string dump_string;
		document.Dump(dump_string);
		logti(dump_string.c_str());
		*/

		// object encoding 얻기
		if (document.GetProperty(object_index)->GetType() == AmfDataType::Object)
		{
			object = (AmfObjectArray *)(document.GetProperty(object_index)->GetObject());
		}
		else
		{
			object = (AmfObjectArray *)(document.GetProperty(object_index)->GetArray());
		}

		// DeviceType
		if ((index = object->FindName("videodevice")) >= 0 && object->GetType(index) == AmfDataType::String)
		{
			_device_string = object->GetString(index);	//DeviceType - XSplit
		}
		else if ((index = object->FindName("encoder")) >= 0 && object->GetType(index) == AmfDataType::String)
		{
			_device_string = object->GetString(index);
		}

		// Encoder
		if (_device_string.IndexOf("Open Broadcaster") >= 0)
		{
			encoder_type = RtmpEncoderType::OBS;
		}
		else if (_device_string.IndexOf("obs-output") >= 0)
		{
			encoder_type = RtmpEncoderType::OBS;
		}
		else if (_device_string.IndexOf("XSplitBroadcaster") >= 0)
		{
			encoder_type = RtmpEncoderType::Xsplit;
		}
		else if (_device_string.IndexOf("Lavf") >= 0)
		{
			encoder_type = RtmpEncoderType::Lavf;
		}
		else
		{
			encoder_type = RtmpEncoderType::Custom;
		}

		// Video Codec
		if ((index = object->FindName("videocodecid")) >= 0)
		{
			video_Available = true;
			if (object->GetType(index) == AmfDataType::String && strcmp("avc1", object->GetString(index)) == 0)
			{
				video_codec_type = RtmpCodecType::H264;
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("H264Avc", object->GetString(index)) == 0)
			{
				video_codec_type = RtmpCodecType::H264;
			}
			else if (object->GetType(index) == AmfDataType::Number && object->GetNumber(index) == 7.0)
			{
				video_codec_type = RtmpCodecType::H264;
			}
		}

		// Video Framerate
		if ((index = object->FindName("framerate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			frame_rate = object->GetNumber(index);
		}
		else if ((index = object->FindName("videoframerate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			frame_rate = object->GetNumber(index);
		}

		// Video Width
		if ((index = object->FindName("width")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			video_width = object->GetNumber(index);
		}

		// Video Height
		if ((index = object->FindName("height")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			video_height = object->GetNumber(index);
		}

		// Video Bitrate
		if ((index = object->FindName("videodatarate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			video_bitrate = object->GetNumber(index);
		}  // Video Data Rate
		if ((index = object->FindName("bitrate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			video_bitrate = object->GetNumber(index);
		}  // Video Data Rate
		if (((index = object->FindName("maxBitrate")) >= 0) && object->GetType(index) == AmfDataType::String)
		{
			bitrate_string = object->GetString(index);
			video_bitrate = strtol(bitrate_string.CStr(), nullptr, 0);
		}

		// Audio Codec
		if ((index = object->FindName("audiocodecid")) >= 0)
		{
			audio_Available = true;
			if (object->GetType(index) == AmfDataType::String && strcmp("mp4a", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::AAC;	//AAC
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("mp3", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::MP3;	//MP3
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp(".mp3", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::MP3;	//MP3
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("speex", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::SPEEX;  //Speex
			}
			else if (object->GetType(index) == AmfDataType::Number && object->GetNumber(index) == 10.0)
			{
				audio_codec_type = RtmpCodecType::AAC;	//AAC
			}
			else if (object->GetType(index) == AmfDataType::Number && object->GetNumber(index) == 11.0)
			{
				audio_codec_type = RtmpCodecType::SPEEX;  //Speex
			}
			else if (object->GetType(index) == AmfDataType::Number && object->GetNumber(index) == 2.0)
			{
				audio_codec_type = RtmpCodecType::MP3;
			}  //MP3
		}

		// Audio bitrate
		if ((index = object->FindName("audiodatarate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			audio_bitrate = object->GetNumber(index);  // Audio Data Rate
		}
		else if ((index = object->FindName("audiobitrate")) >= 0 && object->GetType(index) == AmfDataType::Number)
		{
			audio_bitrate = object->GetNumber(index);
		}  // Audio Data Rate

		// Audio Channels
		if ((index = object->FindName("audiochannels")) >= 0)
		{
			if (object->GetType(index) == AmfDataType::Number)
			{
				audio_channels = object->GetNumber(index);
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("stereo", object->GetString(index)) == 0)
			{
				audio_channels = 2;
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("mono", object->GetString(index)) == 0)
			{
				audio_channels = 1;
			}
		}

		// Audio samplerate
		if ((index = object->FindName("audiosamplerate")) >= 0)
		{
			audio_samplerate = object->GetNumber(index);
		}  // Audio Sample Rate

		// Audio samplesize
		if ((index = object->FindName("audiosamplesize")) >= 0)
		{
			audio_samplesize = object->GetNumber(index);
		}  // Audio Sample Size

		if ((video_Available == true && video_codec_type != RtmpCodecType::H264) ||
			(audio_Available == true && audio_codec_type != RtmpCodecType::AAC))
		{
			logtw("AmfMeta has incompatible codec information. - stream(%s/%s) id(%u/%u) video(%s) audio(%s)",
				  _vhost_app_name.CStr(),
				  _stream_name.CStr(),
				  _app_id,
				  GetId(),
				  GetCodecString(video_codec_type).CStr(),
				  GetCodecString(audio_codec_type).CStr());
		}

		_media_info->video_codec_type = video_codec_type;
		_media_info->video_width = (int32_t)video_width;
		_media_info->video_height = (int32_t)video_height;
		_media_info->video_framerate = (float)frame_rate;
		_media_info->video_bitrate = (int32_t)video_bitrate;
		_media_info->audio_codec_type = audio_codec_type;
		_media_info->audio_bitrate = (int32_t)audio_bitrate;
		_media_info->audio_channels = (int32_t)audio_channels;
		_media_info->audio_bits = (int32_t)audio_samplesize;
		_media_info->audio_samplerate = (int32_t)audio_samplerate;
		_media_info->encoder_type = encoder_type;

		return true;
	}

	void RtmpStream::OnAmfDeleteStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		logtd("Delete Stream - stream(%s/%s) id(%u/%u)", _vhost_app_name.CStr(), _stream_name.CStr(), _app_id, GetId());

		_media_info->video_stream_coming = false;
		_media_info->audio_stream_coming = false;

		// it will call PhysicalPort::OnDisconnected
		_remote->Close();
	}

	off_t RtmpStream::ReceiveHandshakePacket(const std::shared_ptr<const ov::Data> &data)
	{
		// +-------------+                           +-------------+
		// |    Client   |       TCP/IP Network      |    Server   |
		// +-------------+            |              +-------------+
		//       |                    |                     |
		// Uninitialized              |               Uninitialized
		//       |          C0        |                     |
		//       |------------------->|         C0          |
		//       |                    |-------------------->|
		//       |          C1        |                     |
		//       |------------------->|         S0          |
		//       |                    |<--------------------|
		//       |                    |         S1          |
		//  Version sent              |<--------------------|
		//       |          S0        |                     |
		//       |<-------------------|                     |
		//       |          S1        |                     |
		//       |<-------------------|                Version sent
		//       |                    |         C1          |
		//       |                    |-------------------->|
		//       |          C2        |                     |
		//       |------------------->|         S2          |
		//       |                    |<--------------------|
		//    Ack sent                |                  Ack Sent
		//       |          S2        |                     |
		//       |<-------------------|                     |
		//       |                    |         C2          |
		//       |                    |-------------------->|
		//  Handshake Done            |               Handshake Done
		//       |                    |                     |
		//
		//           Pictorial Representation of Handshake

		int32_t process_size = 0;
		switch (_handshake_state)
		{
			case RtmpHandshakeState::Uninitialized:
				logtd("Handshaking is started. Trying to parse for C0/C1 packets...");
				process_size = (sizeof(uint8_t) + RTMP_HANDSHAKE_PACKET_SIZE);
				break;

			case RtmpHandshakeState::S2:
				logtd("Trying to parse for C2 packet...");
				process_size = (RTMP_HANDSHAKE_PACKET_SIZE);
				break;

			default:
				logte("Failed to handshake: state: %d", static_cast<int32_t>(_handshake_state));
				return -1LL;
		}

		if (static_cast<int32_t>(data->GetLength()) < process_size)
		{
			// Need more data
			logtd("Need more data: data: %zu bytes, expected: %d bytes", data->GetLength(), process_size);
			return 0LL;
		}

		if (_handshake_state == RtmpHandshakeState::Uninitialized)
		{
			logtd("C0/C1 packets are arrived");

			char version = data->At(0);

			logtd("Trying to check RTMP version (%d)...", RTMP_HANDSHAKE_VERSION);

			if (version != RTMP_HANDSHAKE_VERSION)
			{
				logte("Invalid RTMP version: %d, expected: %d", version, RTMP_HANDSHAKE_VERSION);
				return -1LL;
			}

			_handshake_state = RtmpHandshakeState::C0;

			auto send_packet = data->Clone();

			// S0,S1,S2 전송

			logtd("Trying to send S0/S1/S2 packets...");

			if (SendHandshake(send_packet) == false)
			{
				logte("Could not send S0/S1/S2 packets");
				return -1LL;
			}

			return process_size;
		}

		logtd("C2 packet is arrived");

		_handshake_state = RtmpHandshakeState::C2;
		_handshake_state = RtmpHandshakeState::Complete;

		logtd("Handshake is completed");

		return process_size;
	}

	int32_t RtmpStream::ReceiveChunkPacket(const std::shared_ptr<const ov::Data> &data)
	{
		int32_t process_size = 0;
		size_t import_size = 0;
		std::shared_ptr<const ov::Data> current_data = data;

		while (current_data->IsEmpty() == false)
		{
			auto status = _import_chunk->Parse(current_data, &import_size);

			process_size += import_size;

			switch (status)
			{
				case RtmpChunkParser::ParseResult::Error:
					logte("An error occurred while parse RTMP data");
					return -1;

				case RtmpChunkParser::ParseResult::NeedMoreData:
					break;

				case RtmpChunkParser::ParseResult::Parsed:
					if (ReceiveChunkMessage() == false)
					{
						logtd("ReceiveChunkMessage Fail");
						logtp("Failed to import packet\n%s", current_data->Dump(current_data->GetLength()).CStr());

						return -1LL;
					}
					break;
			}

			current_data = current_data->Subdata(import_size);

			if (status == RtmpChunkParser::ParseResult::NeedMoreData)
			{
				break;
			}
		}

		return process_size;
	}

	bool RtmpStream::ReceiveChunkMessage()
	{
		// Transact chunk message
		while (true)
		{
			auto message = _import_chunk->GetMessage();

			if ((message == nullptr) || (message->payload == nullptr))
			{
				break;
			}

			bool result = true;

			switch (message->header->completed.type_id)
			{
				case RtmpMessageTypeID::AUDIO:
					result = ReceiveAudioMessage(message);
					break;
				case RtmpMessageTypeID::VIDEO:
					result = ReceiveVideoMessage(message);
					break;
				case RtmpMessageTypeID::SET_CHUNK_SIZE:
					result = ReceiveSetChunkSize(message);
					break;
				case RtmpMessageTypeID::AMF0_DATA:
					ReceiveAmfDataMessage(message);
					break;
				case RtmpMessageTypeID::AMF0_COMMAND:
					ReceiveAmfCommandMessage(message);
					break;
				case RtmpMessageTypeID::USER_CONTROL:
					result = ReceiveUserControlMessage(message);
					break;
				case RtmpMessageTypeID::WINDOW_ACKNOWLEDGEMENT_SIZE:
					ReceiveWindowAcknowledgementSize(message);
					break;
				default:
					logtw("Unknown Type - Type(%d)", message->header->completed.type_id);
					break;
			}

			if (!result)
			{
				return false;
			}
		}

		return true;
	}

	bool RtmpStream::ReceiveSetChunkSize(const std::shared_ptr<const RtmpMessage> &message)
	{
		auto chunk_size = RtmpMuxUtil::ReadInt32(message->payload->GetData());

		logti("[%s/%s] ChunkSize is changed to %u (stream id: %u)", _vhost_app_name.CStr(), _stream_name.CStr(), chunk_size, message->header->completed.stream_id);

		_import_chunk->SetChunkSize(chunk_size);

		return true;
	}

	bool RtmpStream::ReceiveUserControlMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		auto data = message->payload;

		// RTMP Spec. v1.0 - 6.2. User Control Messages
		//
		// User Control messages SHOULD use message stream ID 0 (known as the
		// control stream) and, when sent over RTMP Chunk Stream, be sent on
		// chunk stream ID 2.
		auto message_stream_id = message->header->completed.stream_id;
		auto chunk_stream_id = message->header->basic_header.chunk_stream_id;
		if (
			(message_stream_id != 0) ||
			(chunk_stream_id != 2))
		{
			logte("Invalid id (message stream id: %u, chunk stream id: %u)", message_stream_id, chunk_stream_id);
			return false;
		}

		if (data->GetLength() < 2)
		{
			logte("Invalid user control message size (data length must greater than 2 bytes, but %zu)", data->GetLength());
			return false;
		}

		ov::ByteStream byte_stream(data);

		auto type = byte_stream.ReadBE16();

		switch (type)
		{
			case RTMP_UCMID_PINGREQUEST: {
				if (data->GetLength() != 6)
				{
					logte("Invalid ping message size: %zu", data->GetLength());
					return false;
				}

				// +---------------+--------------------------------------------------+
				// | PingResponse  | The client sends this event to the server in     |
				// | (=7)          | response to the ping request. The event data is  |
				// |               | a 4-byte timestamp, which was received with the  |
				// |               | PingRequest request.                             |
				// +---------------+--------------------------------------------------+
				// ping response == event type (16 bits) + timestamp (32 bits)
				auto body = std::make_shared<std::vector<uint8_t>>(2 + 4);
				auto write_buffer = body->data();
				auto message_header = std::make_shared<RtmpMuxMessageHeader>(
					chunk_stream_id,
					0,
					RtmpMessageTypeID::USER_CONTROL,
					message_stream_id,
					6);

				*(reinterpret_cast<uint16_t *>(write_buffer)) = ov::HostToBE16(RTMP_UCMID_PINGRESPONSE);
				write_buffer += sizeof(uint16_t);
				*(reinterpret_cast<uint32_t *>(write_buffer)) = ov::HostToBE32(byte_stream.ReadBE32());

				return SendMessagePacket(message_header, body);
			}
		}

		return true;
	}

	void RtmpStream::ReceiveWindowAcknowledgementSize(const std::shared_ptr<const RtmpMessage> &message)
	{
		auto acknowledgement_size = RtmpMuxUtil::ReadInt32(message->payload->GetData());

		if (acknowledgement_size != 0)
		{
			_acknowledgement_size = acknowledgement_size / 2;
		}
	}

	void RtmpStream::ReceiveAmfCommandMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		AmfDocument document;
		ov::String message_name;
		double transaction_id = 0.0;

		OV_ASSERT2(message->header != nullptr);
		OV_ASSERT2(message->payload != nullptr);

		if (document.Decode(message->payload->GetData(), message->header->message_length) == 0)
		{
			logte("AmfDocument Size 0 ");
			return;
		}

		// Message Name
		if (document.GetProperty(0) == nullptr || document.GetProperty(0)->GetType() != AmfDataType::String)
		{
			logte("Message Name Fail");
			return;
		}
		message_name = document.GetProperty(0)->GetString();

		// Message Transaction ID 얻기
		if (document.GetProperty(1) != nullptr && document.GetProperty(1)->GetType() == AmfDataType::Number)
		{
			transaction_id = document.GetProperty(1)->GetNumber();
		}

		// 처리
		if (message_name == RTMP_CMD_NAME_CONNECT)
		{
			OnAmfConnect(message->header, document, transaction_id);
		}
		else if (message_name == RTMP_CMD_NAME_CREATESTREAM)
		{
			OnAmfCreateStream(message->header, document, transaction_id);
		}
		else if (message_name == RTMP_CMD_NAME_FCPUBLISH)
		{
			OnAmfFCPublish(message->header, document, transaction_id);
		}
		else if (message_name == RTMP_CMD_NAME_PUBLISH)
		{
			OnAmfPublish(message->header, document, transaction_id);
		}
		else if (message_name == RTMP_CMD_NAME_RELEASESTREAM)
		{
		}
		else if (message_name == RTMP_PING)
		{
		}
		else if (message_name == RTMP_CMD_NAME_DELETESTREAM)
		{
			//TODO(Dimiden): Check this message, This causes the stream to be deleted twice.
			//OnAmfDeleteStream(message->header, document, transaction_id);
		}
		else
		{
			logtd("Unknown Amf0CommandMessage - Message(%s:%.1f)", message_name.CStr(), transaction_id);
			return;
		}
	}

	void RtmpStream::ReceiveAmfDataMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		AmfDocument document;
		int32_t decode_length = 0;
		ov::String message_name;
		ov::String data_name;

		decode_length = document.Decode(message->payload->GetData(), message->header->message_length);
		if (decode_length == 0)
		{
			logte("Amf0DataMessage Document Length 0");
			return;
		}

		// Message Name 얻기
		if (document.GetProperty(0) != nullptr && document.GetProperty(0)->GetType() == AmfDataType::String)
		{
			message_name = document.GetProperty(0)->GetString();
		}

		// Data 이름 얻기
		if (document.GetProperty(1) != nullptr && document.GetProperty(1)->GetType() == AmfDataType::String)
		{
			data_name = document.GetProperty(1)->GetString();
		}

		// 처리
		if (message_name == RTMP_CMD_DATA_SETDATAFRAME &&
			data_name == RTMP_CMD_DATA_ONMETADATA &&
			document.GetProperty(2) != nullptr &&
			(document.GetProperty(2)->GetType() == AmfDataType::Object ||
			 document.GetProperty(2)->GetType() == AmfDataType::Array))
		{
			OnAmfMetaData(message->header, document, 2);
		}
		else if (message_name == RTMP_CMD_DATA_ONMETADATA &&
				 document.GetProperty(1) != nullptr &&
				 (document.GetProperty(1)->GetType() == AmfDataType::Object ||
				  document.GetProperty(1)->GetType() == AmfDataType::Array))
		{
			OnAmfMetaData(message->header, document, 1);
		}
		else if (message_name == RTMP_CMD_NAME_ONFI)
		{
			// Not support yet
		}
		else
		{
			// Find it in Events
			if (CheckEventMessage(message->header, document) == false)
			{
				logtw("Unknown Amf0DataMessage - Message(%s / %s)", message_name.CStr(), data_name.CStr());
				return;
			}
		}
	}

	bool RtmpStream::CheckEventMessage(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document)
	{
		bool found = false;

		for (const auto &event : _event_generator.GetEvents())
		{
			auto trigger = event.GetTrigger();
			auto trigger_list = trigger.Split(".");

			// Trigger length must be 3 or more
			// AMFDataMessage.[<Property>.<Property>...<Property>.]<Object Name>.<Key Name>
			if (trigger_list.size() < 3)
			{
				logtd("Invalid trigger: %s", trigger.CStr());
				continue;
			}

			if (header->completed.type_id == RtmpMessageTypeID::AMF0_DATA && trigger_list.at(0) == "AMFDataMessage")
			{
				for (std::size_t i = 1; i < trigger_list.size(); i++)
				{
					auto property = document.GetProperty(i - 1);
					if (property == nullptr)
					{
						logtd("Document has no property at %d: %s", i - 1, trigger.CStr());
						break;
					}

					// if last item is must be object or array
					if (i == trigger_list.size() - 1)
					{
						if (property->GetType() == AmfDataType::Object || property->GetType() == AmfDataType::Array)
						{
							auto object = property->GetObject();
							if (object == nullptr)
							{
								logtd("Property is not object: %s", property->GetString());
								break;
							}

							auto key = trigger_list.at(i);
							int32_t index = 0;
							if ((index = object->FindName(key.CStr())) >= 0 && object->GetType(index) == AmfDataType::String)
							{
								found = true;
								auto value = object->GetString(index);
								GenerateEvent(event, value);
							}
						}
						else if (property->GetType() == AmfDataType::String)
						{
							found = true;
							auto value = property->GetString();
							GenerateEvent(event, value);
						}
						else
						{
							logtd("Document property type mismatch at %d: %s", i - 1, property->GetString());
							break;
						}
					}
					else
					{
						if (trigger_list.at(i) != property->GetString())
						{
							logtd("Document property mismatch at %d: %s != %s", i - 1, trigger_list.at(i).CStr(), property->GetString());
							break;
						}
					}
				}
			}
		}

		return found;
	}

	void RtmpStream::GenerateEvent(const cfg::vhost::app::pvd::Event &event, const ov::String &value)
	{
		logtd("Event generated: %s / %s", event.GetTrigger().CStr(), value.CStr());

		bool id3_enabled = false;
		auto id3v2_event = event.GetHLSID3v2(&id3_enabled);

		if (id3_enabled == true)
		{
			ID3v2 tag;
			tag.SetVersion(4, 0);

			if (id3v2_event.GetFrameType() == "TXXX")
			{
				auto info = id3v2_event.GetInfo();
				auto data = id3v2_event.GetData();

				if (info == "${TriggerValue}")
				{
					info = value;
				}
				else if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2TxxxFrame>(info, data));
			}
			else if (id3v2_event.GetFrameType().UpperCaseString().Get(0) == 'T')
			{
				auto data = id3v2_event.GetData();

				if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2TextFrame>(id3v2_event.GetFrameType(), data));
			}
			// PRIV
			else if (id3v2_event.GetFrameType() == "PRIV")
			{
				auto owner = id3v2_event.GetInfo();
				auto data = id3v2_event.GetData();

				if (owner == "${TriggerValue}")
				{
					owner = value;
				}
				else if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2PrivFrame>(owner, data));
			}
			else
			{
				logtw("Unsupported ID3v2 frame type: %s", id3v2_event.GetFrameType().CStr());
				return;
			}

			cmn::PacketType packet_type = cmn::PacketType::EVENT;
			if (id3v2_event.GetEventType().LowerCaseString() == "video")
			{
				packet_type = cmn::PacketType::VIDEO_EVENT;
			}
			else if (id3v2_event.GetEventType().LowerCaseString() == "audio")
			{
				packet_type = cmn::PacketType::AUDIO_EVENT;
			}
			else if (id3v2_event.GetEventType().LowerCaseString() == "event")
			{
				packet_type = cmn::PacketType::EVENT;
			}
			else
			{
				logtw("Unsupported inject type: %s", id3v2_event.GetEventType().CStr());
				return;
			}

			int64_t pts = 0;
			if (packet_type == cmn::PacketType::VIDEO_EVENT)
			{
				pts = _last_video_pts;
				pts += _last_video_pts_clock.Elapsed();
			}
			else if (packet_type == cmn::PacketType::AUDIO_EVENT)
			{
				pts = _last_audio_pts;
				pts += _last_audio_pts_clock.Elapsed();
			}

			SendDataFrame(pts, cmn::BitstreamFormat::ID3v2, packet_type, tag.Serialize());
		}
	}

	bool RtmpStream::CheckReadyToPublish()
	{
		if (_media_info->video_stream_coming == true && _media_info->audio_stream_coming == true)
		{
			return true;
		}
		// If this algorithm causes latency, it may be better to use meta data from AmfMeta.
		// But AmfMeta is not always correct so we need more consideration

		// It makes this stream video only
		else if (_media_info->video_stream_coming == true && _stream_message_cache_video_count > MAX_STREAM_MESSAGE_COUNT / 2)
		{
			return true;
		}
		// It makes this stream audio only
		else if (_media_info->audio_stream_coming == true && _stream_message_cache_audio_count > MAX_STREAM_MESSAGE_COUNT / 2)
		{
			return true;
		}

		return false;
	}

	//====================================================================================================
	// Chunk Message - Video Message
	// * Packet structure
	// 1.Control Header(1Byte - Frame Type + Codec Type)
	// 2.Type : 1Byte( 0x00 - Config Packet/ 0x00 -Frame Packet)
	// 3.Composition Time Offset : 3Byte
	// 4.Frame Size(4Byte)
	//
	// * Frame process
	// 1. SPS/PPS information
	// 2. SEI + I-Frame
	// 3. I-Frame/P-Frame repetition
	//
	// 0x06 :  NAL_SEI  first frame, uuid and codec information
	// 0x67 : NAL_SPS
	// 0x68 : NAL_PPS
	// 0x41 : NAL_SLICE -> from basic frame
	// 0x01 : NAL_SLICE -> from basic frame
	// 0x65 : NAL_SLICE_IDR -> from key frame
	//====================================================================================================
	bool RtmpStream::ReceiveVideoMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		if (message->header->message_length == 0)
		{
			// Nothing to do
			logtw("0-byte video message received: stream(%s/%s)",
				  _vhost_app_name.CStr(),
				  _stream_name.CStr());
			return true;
		}

		// size check
		if ((message->header->message_length < RTMP_VIDEO_DATA_MIN_SIZE) ||
			(message->header->message_length > RTMP_MAX_PACKET_SIZE))
		{
			logte("Invalid payload size: stream(%s/%s) size(%d)",
				  _vhost_app_name.CStr(), _stream_name.CStr(),
				  message->header->message_length);
			return false;
		}

		const std::shared_ptr<const ov::Data> &payload = message->payload;

		if (!IsPublished())
		{
			_media_info->video_stream_coming = true;

			if (CheckReadyToPublish() == true)
			{
				if (PublishStream() == false)
				{
					logte("Input create fail -  stream(%s/%s)", _vhost_app_name.CStr(), _stream_name.CStr());
					return false;
				}
			}
			else
			{
				// Store stream data until stream is published
				_stream_message_cache.push_back(message);
				_stream_message_cache_video_count++;
				if (_stream_message_cache.size() > MAX_STREAM_MESSAGE_COUNT)
				{
					logtw("Rtmp input stream init message count over -  stream(%s/%s) size(%d:%d)",
						  _vhost_app_name.CStr(),
						  _stream_name.CStr(),
						  _stream_message_cache.size(),
						  MAX_STREAM_MESSAGE_COUNT);
				}

				return true;
			}
		}

		// video stream callback
		if (_media_info->video_stream_coming)
		{
			// Parsing FLV
			FlvVideoData flv_video;
			if (FlvVideoData::Parse(message->payload->GetDataAs<uint8_t>(), message->payload->GetLength(), flv_video) == false)
			{
				logte("Could not parse flv video (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			if (flv_video.CompositionTime() < 0L)
			{
				if (_negative_cts_detected == false)
				{
					logtw("A negative CTS has been detected and will attempt to adjust the PTS. HLS/DASH may not play smoothly in the beginning.");
					_negative_cts_detected = true;
				}
			}

			int64_t dts = message->header->completed.timestamp;
			int64_t pts = dts + flv_video.CompositionTime();

			if (_negative_cts_detected)
			{
				pts += ADJUST_PTS;
			}

			auto video_track = GetTrack(RTMP_VIDEO_TRACK_ID);
			if (video_track == nullptr)
			{
				logte("Cannot get video track (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			dts *= video_track->GetVideoTimestampScale();
			pts *= video_track->GetVideoTimestampScale();

			if (_is_incoming_timestamp_used == false)
			{
				AdjustTimestamp(pts, dts);
			}

			cmn::PacketType packet_type = cmn::PacketType::Unknown;
			if (flv_video.PacketType() == FlvAvcPacketType::AVC_SEQUENCE_HEADER)
			{
				packet_type = cmn::PacketType::SEQUENCE_HEADER;
			}
			else if (flv_video.PacketType() == FlvAvcPacketType::AVC_NALU)
			{
				packet_type = cmn::PacketType::NALU;
			}
			else if (flv_video.PacketType() == FlvAvcPacketType::AVC_END_SEQUENCE)
			{
				// what can I do?
				return true;
			}

			auto data = std::make_shared<ov::Data>(flv_video.Payload(), flv_video.PayloadLength());
			auto video_frame = std::make_shared<MediaPacket>(GetMsid(),
															 cmn::MediaType::Video,
															 RTMP_VIDEO_TRACK_ID,
															 data,
															 pts,
															 dts,
															 cmn::BitstreamFormat::H264_AVCC,  // RTMP's packet type is AVCC
															 packet_type);

			SendFrame(video_frame);

			// logtc("Video packet sent - stream(%s/%s) type(%d) size(%d) pts(%lld) dts(%lld)",
			// 	  _vhost_app_name.CStr(),
			// 	  _stream_name.CStr(),
			// 	  flv_video.PacketType(),
			// 	  flv_video.PayloadLength(),
			// 	  pts,
			// 	  dts);

			_last_video_pts = dts;

			if (_last_video_pts_clock.IsStart() == false)
			{
				_last_video_pts_clock.Start();
			}
			else
			{
				_last_video_pts_clock.Update();
			}

			// Statistics for debugging
			if (flv_video.FrameType() == FlvVideoFrameTypes::KEY_FRAME)
			{
				_key_frame_interval = message->header->completed.timestamp - _previous_key_frame_timestamp;
				_previous_key_frame_timestamp = message->header->completed.timestamp;
				video_frame->SetFlag(MediaPacketFlag::Key);
			}
			else
			{
				video_frame->SetFlag(MediaPacketFlag::NoFlag);
			}

			_last_video_timestamp = message->header->completed.timestamp;
			_video_frame_count++;

			time_t current_time = time(nullptr);
			uint32_t check_gap = current_time - _stream_check_time;

			if (check_gap >= 10)
			{
				logi("RTMPProvider.Stat", "Rtmp Provider Info - stream(%s/%s) key(%ums) timestamp(v:%ums/a:%ums/g:%dms) fps(v:%u/a:%u) gap(v:%ums/a:%ums)",
					 _vhost_app_name.CStr(),
					 _stream_name.CStr(),
					 _key_frame_interval,
					 _last_video_timestamp,
					 _last_audio_timestamp,
					 _last_video_timestamp - _last_audio_timestamp,
					 _video_frame_count / check_gap,
					 _audio_frame_count / check_gap,
					 _last_video_timestamp - _previous_last_video_timestamp,
					 _last_audio_timestamp - _previous_last_audio_timestamp);

				_stream_check_time = time(nullptr);
				_video_frame_count = 0;
				_audio_frame_count = 0;
				_previous_last_video_timestamp = _last_video_timestamp;
				_previous_last_audio_timestamp = _last_audio_timestamp;
			}
		}

		return true;
	}

	//====================================================================================================
	// Chunk Message - Audio Message
	// * Packet structure
	// 1. AAC
	// - Control Header : 	1Byte(Codec/SampleRate/SampleSize/Channels)
	// - Sequence Type : 	1Byte(0x00 - Config Packet/ 0x00 -Frame Packet)
	// - Frame
	//
	// 2.Speex/MP3
	// - Control Header(1Byte - Codec/SampleRate/SampleSize/Channels)
	// - Frame
	//
	//AAC : Adts header information
	//Speex : size information
	//====================================================================================================
	bool RtmpStream::ReceiveAudioMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		if (message->header->message_length == 0)
		{
			// Nothing to do
			logtw("0-byte audio message received: stream(%s/%s)",
				  _vhost_app_name.CStr(),
				  _stream_name.CStr());
			return true;
		}

		// size check
		if ((message->header->message_length < RTMP_AAC_AUDIO_DATA_MIN_SIZE) ||
			(message->header->message_length > RTMP_MAX_PACKET_SIZE))
		{
			logte("Invalid payload size: stream(%s/%s) size(%d)",
				  _vhost_app_name.CStr(),
				  _stream_name.CStr(),
				  message->header->message_length);

			return false;
		}

		if (!IsPublished())
		{
			_media_info->audio_stream_coming = true;

			if (CheckReadyToPublish() == true)
			{
				if (PublishStream() == false)
				{
					logte("Input create fail -  stream(%s/%s)", _vhost_app_name.CStr(), _stream_name.CStr());
					return false;
				}
			}
			else
			{
				// Store stream data until stream is published
				_stream_message_cache.push_back(message);
				_stream_message_cache_audio_count++;
				if (_stream_message_cache.size() > MAX_STREAM_MESSAGE_COUNT)
				{
					logtw("Rtmp input stream init message count over -  stream(%s/%s) size(%d:%d)",
						  _vhost_app_name.CStr(),
						  _stream_name.CStr(),
						  _stream_message_cache.size(),
						  MAX_STREAM_MESSAGE_COUNT);
				}

				return true;
			}
		}

		// audio stream callback
		if (_media_info->audio_stream_coming)
		{
			// Parsing FLV
			FlvAudioData flv_audio;
			if (FlvAudioData::Parse(message->payload->GetDataAs<uint8_t>(), message->payload->GetLength(), flv_audio) == false)
			{
				logte("Could not parse flv audio (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			int64_t dts = message->header->completed.timestamp;
			int64_t pts = dts;

			if (_negative_cts_detected)
			{
				pts += ADJUST_PTS;
			}

			// Get audio track info
			auto audio_track = GetTrack(RTMP_AUDIO_TRACK_ID);
			if (audio_track == nullptr)
			{
				logte("Cannot get video track (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			pts *= audio_track->GetAudioTimestampScale();
			dts *= audio_track->GetAudioTimestampScale();

			if (_is_incoming_timestamp_used == false)
			{
				AdjustTimestamp(pts, dts);
			}

			cmn::PacketType packet_type = cmn::PacketType::Unknown;
			if (flv_audio.PacketType() == FlvAACPacketType::SEQUENCE_HEADER)
			{
				packet_type = cmn::PacketType::SEQUENCE_HEADER;
			}
			else if (flv_audio.PacketType() == FlvAACPacketType::RAW)
			{
				packet_type = cmn::PacketType::RAW;
			}

			auto data = std::make_shared<ov::Data>(flv_audio.Payload(), flv_audio.PayloadLength());
			auto frame = std::make_shared<MediaPacket>(GetMsid(),
													   cmn::MediaType::Audio,
													   RTMP_AUDIO_TRACK_ID,
													   data,
													   pts,
													   dts,
													   cmn::BitstreamFormat::AAC_RAW,
													   packet_type);

			SendFrame(frame);

			_last_audio_pts = dts;

			if (_last_audio_pts_clock.IsStart() == false)
			{
				_last_audio_pts_clock.Start();
			}
			else
			{
				_last_audio_pts_clock.Update();
			}

			_last_audio_timestamp = message->header->completed.timestamp;
			_audio_frame_count++;
		}

		return true;
	}

	// Make PTS/DTS of first frame are 0
	void RtmpStream::AdjustTimestamp(int64_t &pts, int64_t &dts)
	{
		if (_first_frame == true)
		{
			_first_frame = false;
			_first_pts_offset = pts;
			_first_dts_offset = dts;
		}

		pts -= _first_pts_offset;
		dts -= _first_dts_offset;
	}

	bool RtmpStream::PublishStream()
	{
		if (_publish_url == nullptr)
		{
			logte("Publish url is not set, stream(%s/%s)", _vhost_app_name.CStr(), _stream_name.CStr());
			return false;
		}

		_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(_publish_url->Host(), _publish_url->App());
		_stream_name = _publish_url->Stream();

		// Get application config
		if (GetProvider() == nullptr)
		{
			logte("Could not find provider: %s/%s", _vhost_app_name.CStr(), _stream_name.CStr());
			return false;
		}

		auto application = GetProvider()->GetApplicationByName(_vhost_app_name);
		if (application == nullptr)
		{
			logte("Could not find application: %s/%s", _vhost_app_name.CStr(), _stream_name.CStr());
			Stop();
			return false;
		}

		_event_generator = application->GetConfig().GetProviders().GetRtmpProvider().GetEventGenerator();
		_is_incoming_timestamp_used = application->GetConfig().GetProviders().GetRtmpProvider().IsIncomingTimestampUsed();

		SetName(_publish_url->Stream());

		// Set Track Info
		SetTrackInfo(_media_info);

		// Publish
		if (PublishChannel(_vhost_app_name) == false)
		{
			Stop();
			return false;
		}

#if 0
		// Keep Alive Data Channel
		_event_test_timer.Push(
		[this](void *parameter) -> ov::DelayQueueAction {
			
			for (const auto &event : _event_generator.GetEvents())
			{
				if (event.GetTrigger() == "KEEP-ALIVE")
				{
					GenerateEvent(event, "KEEP-ALIVE");
				}
			}

			return ov::DelayQueueAction::Repeat;
		},
		500);
		_event_test_timer.Start();
#endif

		//   stored messages
		for (auto message : _stream_message_cache)
		{
			if (message->header->completed.type_id == RtmpMessageTypeID::VIDEO && _media_info->video_stream_coming)
			{
				ReceiveVideoMessage(message);
			}
			else if (message->header->completed.type_id == RtmpMessageTypeID::AUDIO && _media_info->audio_stream_coming)
			{
				ReceiveAudioMessage(message);
			}
		}
		_stream_message_cache.clear();

		return true;
	}

	bool RtmpStream::SetTrackInfo(const std::shared_ptr<RtmpMediaInfo> &media_info)
	{
		if (media_info->video_stream_coming)
		{
			auto new_track = std::make_shared<MediaTrack>();

			new_track->SetId(RTMP_VIDEO_TRACK_ID);
			new_track->SetMediaType(cmn::MediaType::Video);
			new_track->SetCodecId(cmn::MediaCodecId::H264);
			new_track->SetOriginBitstream(cmn::BitstreamFormat::H264_AVCC);
			new_track->SetTimeBase(1, 1000);
			new_track->SetVideoTimestampScale(1.0);

			// Below items are not mandatory, it will be parsed again from SPS parser
			new_track->SetWidth((uint32_t)media_info->video_width);
			new_track->SetHeight((uint32_t)media_info->video_height);
			new_track->SetFrameRateByConfig(media_info->video_framerate);

			// Kbps -> bps, it is just metadata
			new_track->SetBitrateByConfig(media_info->video_bitrate * 1000);

			AddTrack(new_track);
		}

		if (media_info->audio_stream_coming)
		{
			auto new_track = std::make_shared<MediaTrack>();

			new_track->SetId(RTMP_AUDIO_TRACK_ID);
			new_track->SetMediaType(cmn::MediaType::Audio);
			new_track->SetCodecId(cmn::MediaCodecId::Aac);
			new_track->SetOriginBitstream(cmn::BitstreamFormat::AAC_RAW);
			new_track->SetTimeBase(1, 1000);
			new_track->SetAudioTimestampScale(1.0);

			//////////////////
			// Below items are not mandatory, it will be parsed again from ADTS parser
			//////////////////
			new_track->SetSampleRate(media_info->audio_samplerate);
			new_track->GetSample().SetFormat(cmn::AudioSample::Format::S16);
			// Kbps -> bps
			new_track->SetBitrateByConfig(media_info->audio_bitrate * 1000);
			// new_track->SetSampleSize(conn->_audio_samplesize);

			if (media_info->audio_channels == 1)
			{
				new_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutMono);
			}
			else if (media_info->audio_channels == 2)
			{
				new_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutStereo);
			}

			AddTrack(new_track);
		}

		// Data Track
		if (GetFirstTrackByType(cmn::MediaType::Data) == nullptr)
		{
			auto data_track = std::make_shared<MediaTrack>();

			data_track->SetId(RTMP_DATA_TRACK_ID);
			data_track->SetMediaType(cmn::MediaType::Data);
			data_track->SetTimeBase(1, 1000);
			data_track->SetOriginBitstream(cmn::BitstreamFormat::ID3v2);

			AddTrack(data_track);
		}

		return true;
	}

	bool RtmpStream::SendData(const std::shared_ptr<const ov::Data> &data)
	{
		if (data == nullptr)
		{
			return false;
		}

		return _remote->Send(data);
	}

	bool RtmpStream::SendData(const void *data, size_t data_size)
	{
		if (data == nullptr)
		{
			return false;
		}

		return _remote->Send(data, data_size);
	}

	bool RtmpStream::SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data)
	{
		auto export_data = _export_chunk->ExportStreamData(message_header, data);

		return SendData(export_data->data(), export_data->size());
	}

	//====================================================================================================
	// Send handshake packet
	// s0 + s1 + s2
	//====================================================================================================
	bool RtmpStream::SendHandshake(const std::shared_ptr<const ov::Data> &data)
	{
		uint8_t s0 = 0;
		uint8_t s1[RTMP_HANDSHAKE_PACKET_SIZE] = {
			0,
		};
		uint8_t s2[RTMP_HANDSHAKE_PACKET_SIZE] = {
			0,
		};

		s0 = RTMP_HANDSHAKE_VERSION;
		RtmpHandshake::MakeS1(s1);
		RtmpHandshake::MakeS2(data->GetDataAs<uint8_t>() + sizeof(uint8_t), s2);
		_handshake_state = RtmpHandshakeState::C1;

		// OM-1629 - Elemental Encoder
		ov::Data dataToSend;

		dataToSend.Append(&s0, sizeof(s0));
		dataToSend.Append(s1, sizeof(s1));
		dataToSend.Append(s2, sizeof(s2));

		if (SendData(dataToSend.GetData(), dataToSend.GetLength()) == false)
		{
			logte("Handshake Send Fail");
			return false;
		}

		_handshake_state = RtmpHandshakeState::S2;

		return true;
	}

	bool RtmpStream::SendUserControlMessage(uint16_t message, std::shared_ptr<std::vector<uint8_t>> &data)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT,
																	 0,
																	 RtmpMessageTypeID::USER_CONTROL,
																	 0,
																	 data->size() + 2);

		data->insert(data->begin(), 0);
		data->insert(data->begin(), 0);
		RtmpMuxUtil::WriteInt16(data->data(), message);

		return SendMessagePacket(message_header, data);
	}

	bool RtmpStream::SendWindowAcknowledgementSize(uint32_t size)
	{
		auto body = std::make_shared<std::vector<uint8_t>>(sizeof(int));
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT,
																	 0,
																	 RtmpMessageTypeID::WINDOW_ACKNOWLEDGEMENT_SIZE,
																	 _rtmp_stream_id,
																	 body->size());

		RtmpMuxUtil::WriteInt32(body->data(), size);

		return SendMessagePacket(message_header, body);
	}

	bool RtmpStream::SendAcknowledgementSize(uint32_t acknowledgement_traffic)
	{
		auto body = std::make_shared<std::vector<uint8_t>>(sizeof(int));
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT,
																	 0,
																	 RtmpMessageTypeID::ACKNOWLEDGEMENT,
																	 0,
																	 body->size());

		RtmpMuxUtil::WriteInt32(body->data(), acknowledgement_traffic);

		return SendMessagePacket(message_header, body);
	}

	bool RtmpStream::SendSetPeerBandwidth(uint32_t bandwidth)
	{
		auto body = std::make_shared<std::vector<uint8_t>>(5);
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT,
																	 0,
																	 RtmpMessageTypeID::SET_PEER_BANDWIDTH,
																	 _rtmp_stream_id,
																	 body->size());

		RtmpMuxUtil::WriteInt32(body->data(), bandwidth);
		RtmpMuxUtil::WriteInt8(body->data() + 4, 2);

		return SendMessagePacket(message_header, body);
	}

	bool RtmpStream::SendStreamBegin(uint32_t stream_id)
	{
		auto body = std::make_shared<std::vector<uint8_t>>(4);

		RtmpMuxUtil::WriteInt32(body->data(), stream_id);

		return SendUserControlMessage(RTMP_UCMID_STREAMBEGIN, body);
	}

	bool RtmpStream::SendStreamEnd()
	{
		auto body = std::make_shared<std::vector<uint8_t>>(4);

		RtmpMuxUtil::WriteInt32(body->data(), _rtmp_stream_id);

		return SendUserControlMessage(RTMP_UCMID_STREAMEOF, body);
	}

	bool RtmpStream::SendAmfCommand(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document)
	{
		auto body = std::make_shared<std::vector<uint8_t>>(2048);
		uint32_t body_size = 0;

		if (message_header == nullptr)
		{
			return false;
		}

		// body
		body_size = document.Encode(body->data());

		if (body_size == 0)
		{
			return false;
		}

		message_header->body_size = body_size;
		body->resize(body_size);

		return SendMessagePacket(message_header, body);
	}

	bool RtmpStream::SendAmfConnectResult(uint32_t chunk_stream_id, double transaction_id, double object_encoding)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 0,
																	 0);
		AmfDocument document;
		AmfObject *object = nullptr;
		AmfArray *array = nullptr;

		// _result
		document.AddProperty(RTMP_ACK_NAME_RESULT);
		document.AddProperty(transaction_id);

		// properties
		object = new AmfObject;
		object->AddProperty("fmsVer", "FMS/3,5,2,654");
		object->AddProperty("capabilities", 31.0);
		object->AddProperty("mode", 1.0);

		document.AddProperty(object);

		// information
		object = new AmfObject;
		object->AddProperty("level", "status");
		object->AddProperty("code", "NetConnection.Connect.Success");
		object->AddProperty("description", "Connection succeeded.");
		object->AddProperty("clientid", _client_id);
		object->AddProperty("objectEncoding", object_encoding);

		array = new AmfArray;
		array->AddProperty("version", "3,5,2,654");
		object->AddProperty("data", array);

		document.AddProperty(object);

		return SendAmfCommand(message_header, document);
	}

	bool RtmpStream::SendAmfOnFCPublish(uint32_t chunk_stream_id, uint32_t stream_id, double client_id)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 _rtmp_stream_id,
																	 0);
		AmfDocument document;
		AmfObject *object = nullptr;

		document.AddProperty(RTMP_CMD_NAME_ONFCPUBLISH);
		document.AddProperty(0.0);
		document.AddProperty(AmfDataType::Null);

		object = new AmfObject;
		object->AddProperty("level", "status");
		object->AddProperty("code", "NetStream.Publish.Start");
		object->AddProperty("description", "FCPublish");
		object->AddProperty("clientid", client_id);

		document.AddProperty(object);

		return SendAmfCommand(message_header, document);
	}

	bool RtmpStream::SendAmfCreateStreamResult(uint32_t chunk_stream_id, double transaction_id)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 0,
																	 0);
		AmfDocument document;

		// 스트림ID 정하기
		_rtmp_stream_id = 1;

		document.AddProperty(RTMP_ACK_NAME_RESULT);
		document.AddProperty(transaction_id);
		document.AddProperty(AmfDataType::Null);
		document.AddProperty((double)_rtmp_stream_id);

		return SendAmfCommand(message_header, document);
	}

	bool RtmpStream::SendAmfOnStatus(uint32_t chunk_stream_id,
									 uint32_t stream_id,
									 char *level,
									 char *code,
									 char *description,
									 double client_id)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 stream_id,
																	 0);
		AmfDocument document;
		AmfObject *object = nullptr;

		document.AddProperty(RTMP_CMD_NAME_ONSTATUS);
		document.AddProperty(0.0);
		document.AddProperty(AmfDataType::Null);

		object = new AmfObject;
		object->AddProperty("level", level);
		object->AddProperty("code", code);
		object->AddProperty("description", description);
		object->AddProperty("clientid", client_id);

		document.AddProperty(object);

		return SendAmfCommand(message_header, document);
	}

	ov::String RtmpStream::GetCodecString(RtmpCodecType codec_type)
	{
		ov::String codec_string;

		switch (codec_type)
		{
			case RtmpCodecType::H264:
				codec_string = "h264";
				break;
			case RtmpCodecType::AAC:
				codec_string = "aac";
				break;
			case RtmpCodecType::MP3:
				codec_string = "mp3";
				break;
			case RtmpCodecType::SPEEX:
				codec_string = "speex";
				break;
			case RtmpCodecType::Unknown:
			default:
				codec_string = "unknown";
				break;
		}

		return codec_string;
	}

	ov::String RtmpStream::GetEncoderTypeString(RtmpEncoderType encoder_type)
	{
		ov::String codec_string;

		switch (encoder_type)
		{
			case RtmpEncoderType::Xsplit:
				codec_string = "Xsplit";
				break;
			case RtmpEncoderType::OBS:
				codec_string = "OBS";
				break;
			case RtmpEncoderType::Lavf:
				codec_string = "Lavf/ffmpeg";
				break;
			case RtmpEncoderType::Custom:
				codec_string = "Unknown";
				break;

			default:
				codec_string = "Unknown";
				break;
		}

		return codec_string;
	}
}  // namespace pvd
