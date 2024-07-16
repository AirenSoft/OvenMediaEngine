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
			auto request_info = std::make_shared<ac::RequestInfo>(requested_url, final_url, _remote, nullptr);
			GetProvider()->SendCloseAdmissionWebhooks(request_info);
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

	bool RtmpStream::PostPublish(const AmfDocument &document)
	{
		if (_tc_url.IsEmpty() == false)
		{
			_full_url = _tc_url;
			auto remain = document.GetProperty(3)->GetString();
			if (remain.IsEmpty() == false)
			{
				_full_url += ov::String::FormatString("/%s", remain.CStr());
			}

			if (SetFullUrl(_full_url) &&
				CheckAccessControl() &&
				ValidatePublishUrl())
			{
				return true;
			}
		}

		Stop();
		return false;
	}

	void RtmpStream::OnAmfConnect(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		double object_encoding = 0.0;

		auto meta_property = document.GetProperty(2, AmfTypeMarker::Object);
		if (meta_property != nullptr)
		{
			auto object = meta_property->GetObject();

			// object encoding
			auto pair = object->GetPair("objectEncoding", AmfTypeMarker::Number);
			if (pair != nullptr)
			{
				object_encoding = pair->property.GetNumber();
			}

			// app name set
			pair = object->GetPair("app", AmfTypeMarker::String);
			if (pair != nullptr)
			{
				_app_name = pair->property.GetString();
			}

			// app url set
			pair = object->GetPair("tcUrl", AmfTypeMarker::String);
			if (pair != nullptr)
			{
				_tc_url = pair->property.GetString();
			}
		}

		// Parse the URL to obtain the domain name
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

		if (SendWindowAcknowledgementSize(RTMP_DEFAULT_ACKNOWNLEDGEMENT_SIZE) == false)
		{
			logte("SendWindowAcknowledgementSize Fail");
			return;
		}

		if (SendSetPeerBandwidth(_peer_bandwidth) == false)
		{
			logte("SendSetPeerBandwidth Fail");
			return;
		}

		if (SendStreamBegin(0) == false)
		{
			logte("SendStreamBegin Fail");
			return;
		}

		if (SendAmfConnectResult(header->basic_header.chunk_stream_id, transaction_id, object_encoding) == false)
		{
			logte("SendAmfConnectResult Fail");
			return;
		}
	}

	void RtmpStream::OnAmfCreateStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (SendAmfCreateStreamResult(header->basic_header.chunk_stream_id, transaction_id) == false)
		{
			logte("SendAmfCreateStreamResult Fail");
			return;
		}
	}

	bool RtmpStream::CheckAccessControl()
	{
		// Check SignedPolicy
		auto provider = GetProvider();

		auto request_info = std::make_shared<ac::RequestInfo>(_url, nullptr, _remote, nullptr);

		auto [signed_policy_result, _signed_policy] = provider->VerifyBySignedPolicy(request_info);
		switch (signed_policy_result)
		{
			case AccessController::VerificationResult::Error:
				logtw("SingedPolicy error : %s", _url->ToUrlString().CStr());
				Stop();
				return false;

			case AccessController::VerificationResult::Fail:
				logtw("%s", _signed_policy->GetErrMessage().CStr());
				Stop();
				return false;

			case AccessController::VerificationResult::Off:
				break;

			case AccessController::VerificationResult::Pass:
				_stream_expired_msec = _signed_policy->GetStreamExpireEpochMSec();
				break;
		}

		auto [webhooks_result, _admission_webhooks] = provider->VerifyByAdmissionWebhooks(request_info);
		switch (webhooks_result)
		{
			case AccessController::VerificationResult::Error:
				logtw("AdmissionWebhooks error : %s", _url->ToUrlString().CStr());
				Stop();
				break;

			case AccessController::VerificationResult::Fail:
				logtw("AdmissionWebhooks error : %s", _admission_webhooks->GetErrReason().CStr());
				Stop();
				break;

			case AccessController::VerificationResult::Off:
				return true;

			case AccessController::VerificationResult::Pass:
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

		return false;
	}

	bool RtmpStream::ValidatePublishUrl()
	{
		if (_publish_url == nullptr)
		{
			logte("Publish URL is not set");
			return false;
		}

		auto scheme = _publish_url->Scheme().UpperCaseString();
		if (((scheme != "RTMP") && (scheme != "RTMPS")) ||
			_publish_url->Host().IsEmpty() ||
			_publish_url->App().IsEmpty() ||
			_publish_url->Stream().IsEmpty())
		{
			logte("Invalid publish URL: %s", _publish_url->ToUrlString().CStr());
			return false;
		}

		auto orchestrator = ocst::Orchestrator::GetInstance();
		auto vhost_app_name = orchestrator->ResolveApplicationNameFromDomain(_publish_url->Host(), _publish_url->App());

		auto app_info = orchestrator->GetApplicationInfo(vhost_app_name);

		if (app_info.IsValid())
		{
			_app_id = app_info.GetId();
			return true;
		}

		logte("Could not find application: %s", vhost_app_name.CStr());
		return false;
	}

	void RtmpStream::OnAmfFCPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty())
		{
			auto property = document.GetProperty(3, AmfTypeMarker::String);

			if (property != nullptr)
			{
				// TODO: check if the chunk stream id is already exist, and generates new rtmp_stream_id and client_id.
				if (SendAmfOnFCPublish(header->basic_header.chunk_stream_id, _rtmp_stream_id, _client_id) == false)
				{
					logte("SendAmfOnFCPublish Fail");
					return;
				}

				PostPublish(document);
			}
		}
	}

	void RtmpStream::OnAmfPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty())
		{
			auto property = document.GetProperty(3, AmfTypeMarker::String);
			if (property != nullptr)
			{
				if (PostPublish(document) == false)
				{
					return;
				}
			}
			else
			{
				logte("OnPublish - Publish Name None");

				//Reject
				SendAmfOnStatus(header->basic_header.chunk_stream_id,
								_rtmp_stream_id,
								"error",
								"NetStream.Publish.Rejected",
								"Authentication Failed.",
								_client_id);

				return;
			}
		}

		_chunk_stream_id = header->basic_header.chunk_stream_id;

		if (SendStreamBegin(_rtmp_stream_id) == false)
		{
			logte("SendStreamBegin Fail");
			return;
		}

		if (SendAmfOnStatus(static_cast<uint32_t>(_chunk_stream_id),
							_rtmp_stream_id,
							"status",
							"NetStream.Publish.Start",
							"Publishing",
							_client_id) == false)
		{
			logte("SendAmfOnStatus Fail");
			return;
		}
	}

	bool RtmpStream::SetFullUrl(ov::String url)
	{
		_url = ov::Url::Parse(url);

		if (_url == nullptr)
		{
			logtw("Could not parse the URL: %s", url.CStr());
			return false;
		}

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

	bool RtmpStream::OnAmfMetaData(const std::shared_ptr<const RtmpChunkHeader> &header, const AmfProperty *property)
	{
		auto object = (property->GetType() == AmfTypeMarker::Object)
						  ? static_cast<const AmfObjectArray *>(property->GetObject())
						  : static_cast<const AmfObjectArray *>(property->GetEcmaArray());

		// DeviceType
		{
			auto property_pair = object->GetPair("videodevice", AmfTypeMarker::String);

			if (property_pair != nullptr)
			{
				_device_string = property_pair->property.GetString();
			}
			else
			{
				property_pair = object->GetPair("encoder", AmfTypeMarker::String);

				if (property_pair != nullptr)
				{
					_device_string = property_pair->property.GetString();
				}
			}
		}

		// Encoder
		RtmpEncoderType encoder_type = RtmpEncoderType::Custom;
		{
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
		}

		// Video Codec
		bool video_available = false;
		RtmpCodecType video_codec_type = RtmpCodecType::Unknown;
		{
			auto property_pair = object->GetPair("videocodecid");

			if (property_pair != nullptr)
			{
				auto &property = property_pair->property;
				video_available = true;

				switch (property.GetType())
				{
					case AmfTypeMarker::String: {
						auto value = property.GetString();
						if ((value == "avc1") || (value == "H264Avc"))
						{
							video_codec_type = RtmpCodecType::H264;
						}
						break;
					}

					case AmfTypeMarker::Number: {
						auto value = property.GetNumber();
						if (value == 7.0)
						{
							video_codec_type = RtmpCodecType::H264;
						}
						break;
					}

					default:
						break;
				}
			}
		}

		// Video Framerate
		double frame_rate = 30.0;
		{
			auto property_pair = object->GetPair("framerate", AmfTypeMarker::Number);

			if (property_pair != nullptr)
			{
				frame_rate = property_pair->property.GetNumber();
			}
			else
			{
				property_pair = object->GetPair("videoframerate", AmfTypeMarker::Number);

				if (property_pair != nullptr)
				{
					frame_rate = property_pair->property.GetNumber();
				}
			}
		}

		// Video Width
		double video_width = 0;
		{
			auto property_pair = object->GetPair("width", AmfTypeMarker::Number);

			if (property_pair != nullptr)
			{
				video_width = property_pair->property.GetNumber();
			}
		}

		// Video Height
		double video_height = 0;
		{
			auto property_pair = object->GetPair("height", AmfTypeMarker::Number);

			if (property_pair != nullptr)
			{
				video_height = property_pair->property.GetNumber();
			}
		}

		// Video Bitrate
		double video_bitrate = 0;
		{
			auto property_pair = object->GetPair("videodatarate", AmfTypeMarker::Number);

			if (property_pair != nullptr)
			{
				video_bitrate = property_pair->property.GetNumber();
			}
			else
			{
				property_pair = object->GetPair("bitrate", AmfTypeMarker::Number);

				if (property_pair != nullptr)
				{
					video_bitrate = property_pair->property.GetNumber();
				}
			}

			property_pair = object->GetPair("maxBitrate", AmfTypeMarker::String);

			if (property_pair != nullptr)
			{
				video_bitrate = ::strtol(property_pair->property.GetString(), nullptr, 0);
			}
		}

		// Audio Codec
		RtmpCodecType audio_codec_type = RtmpCodecType::Unknown;
		bool audio_available = false;
		{
			auto property_pair = object->GetPair("audiocodecid");

			if (property_pair != nullptr)
			{
				audio_available = true;

				auto &property = property_pair->property;
				auto type = property.GetType();

				switch (type)
				{
					case AmfTypeMarker::String: {
						auto value = property.GetString();
						if (value == "mp4a")
						{
							audio_codec_type = RtmpCodecType::AAC;
						}
						else if (value == "mp3" || value == ".mp3")
						{
							audio_codec_type = RtmpCodecType::MP3;
						}
						else if (value == "speex")
						{
							audio_codec_type = RtmpCodecType::SPEEX;
						}
						break;
					}

					case AmfTypeMarker::Number: {
						auto value = property.GetNumber();
						if (value == 10.0)
						{
							audio_codec_type = RtmpCodecType::AAC;
						}
						else if (value == 11.0)
						{
							audio_codec_type = RtmpCodecType::SPEEX;
						}
						else if (value == 2.0)
						{
							audio_codec_type = RtmpCodecType::MP3;
						}
						break;
					}

					default:
						break;
				}
			}
		}

		// Audio bitrate
		double audio_bitrate = 0.0;
		{
			auto property_pair = object->GetPair("audiodatarate", AmfTypeMarker::Number);

			if (property_pair != nullptr)
			{
				audio_bitrate = property_pair->property.GetNumber();
			}
			else
			{
				property_pair = object->GetPair("audiobitrate", AmfTypeMarker::Number);

				if (property_pair != nullptr)
				{
					audio_bitrate = property_pair->property.GetNumber();
				}
			}
		}

		// Audio Channels
		double audio_channels = 1.0;
		{
			auto property_pair = object->GetPair("audiochannels");

			if (property_pair != nullptr)
			{
				switch (property_pair->property.GetType())
				{
					case AmfTypeMarker::Number:
						audio_channels = property_pair->property.GetNumber();
						break;

					case AmfTypeMarker::String: {
						auto value = property_pair->property.GetString();
						if (value == "stereo")
						{
							audio_channels = 2;
						}
						else if (value == "mono")
						{
							audio_channels = 1;
						}
					}

					default:
						break;
				}
			}
		}

		// Audio samplerate
		double audio_samplerate = 0.0;
		{
			auto property_pair = object->GetPair("audiosamplerate");

			if (property_pair != nullptr)
			{
				audio_samplerate = property_pair->property.GetNumber();
			}
		}

		// Audio samplesize
		double audio_samplesize = 0.0;
		{
			auto property_pair = object->GetPair("audiosamplesize");

			if (property_pair != nullptr)
			{
				audio_samplesize = property_pair->property.GetNumber();
			}
		}

		if (((video_available == true) && (video_codec_type != RtmpCodecType::H264)) ||
			((audio_available == true) && (audio_codec_type != RtmpCodecType::AAC)))
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
		_media_info->video_width = static_cast<int32_t>(video_width);
		_media_info->video_height = static_cast<int32_t>(video_height);
		_media_info->video_framerate = static_cast<float>(frame_rate);
		_media_info->video_bitrate = static_cast<int32_t>(video_bitrate);
		_media_info->audio_codec_type = audio_codec_type;
		_media_info->audio_bitrate = static_cast<int32_t>(audio_bitrate);
		_media_info->audio_channels = static_cast<int32_t>(audio_channels);
		_media_info->audio_bits = static_cast<int32_t>(audio_samplesize);
		_media_info->audio_samplerate = static_cast<int32_t>(audio_samplerate);
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
				case RtmpMessageTypeID::ACKNOWLEDGEMENT:
					// OME doesn't use this message
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
		OV_ASSERT2(message->header != nullptr);
		OV_ASSERT2(message->payload != nullptr);
		OV_ASSERT2(message->payload->GetLength() == message->header->message_length);

		ov::ByteStream byte_stream(message->payload);
		AmfDocument document;

		if (document.Decode(byte_stream) == false)
		{
			logte("Could not decode AMFDocument");
			return;
		}

		// Message Name
		ov::String message_name;
		{
			auto property = document.GetProperty(0, AmfTypeMarker::String);

			if (property == nullptr)
			{
				logte("Message name is not available");
				return;
			}

			message_name = property->GetString();
		}

		// Obtain the Message Transaction ID
		double transaction_id = 0.0;
		{
			auto property = document.GetProperty(1, AmfTypeMarker::Number);

			if (property != nullptr)
			{
				transaction_id = property->GetNumber();
			}
		}

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
		OV_ASSERT2(message->payload->GetLength() == message->header->message_length);

		ov::ByteStream byte_stream(message->payload);
		AmfDocument document;
		auto decode_length = document.Decode(byte_stream);

		if (decode_length == 0)
		{
			logte("Amf0DataMessage Document Length 0");
			return;
		}

		// Obtain the message name
		ov::String message_name;
		auto message_name_property = document.GetProperty(0);
		auto message_name_type = (message_name_property != nullptr) ? message_name_property->GetType() : AmfTypeMarker::Undefined;
		if ((message_name_type == AmfTypeMarker::String))
		{
			message_name = message_name_property->GetString();
		}

		// Obtain the data name
		ov::String data_name;
		auto data_name_property = document.GetProperty(1);
		auto data_name_type = (data_name_property != nullptr) ? data_name_property->GetType() : AmfTypeMarker::Undefined;
		if (data_name_type == AmfTypeMarker::String)
		{
			data_name = data_name_property->GetString();
		}

		auto third_property = document.GetProperty(2);
		auto third_type = (third_property != nullptr) ? third_property->GetType() : AmfTypeMarker::Undefined;

		if (
			(message_name == RTMP_CMD_DATA_SETDATAFRAME) &&
			(data_name == RTMP_CMD_DATA_ONMETADATA) &&
			(third_type == AmfTypeMarker::Object || third_type == AmfTypeMarker::EcmaArray))
		{
			OnAmfMetaData(message->header, third_property);
		}
		else if (
			(message_name == RTMP_CMD_DATA_ONMETADATA) &&
			(data_name_type == AmfTypeMarker::Object || data_name_type == AmfTypeMarker::EcmaArray))
		{
			OnAmfMetaData(message->header, data_name_property);
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

			if (
				(header->completed.type_id == RtmpMessageTypeID::AMF0_DATA) &&
				(trigger_list.at(0) == "AMFDataMessage"))
			{
				auto count = trigger_list.size();

				for (std::size_t size = 1; size < count; size++)
				{
					auto property = document.GetProperty(size - 1);

					if (property == nullptr)
					{
						logtd("Document has no property at %d: %s", size - 1, trigger.CStr());
						break;
					}

					auto type = property->GetType();

					// if last item is must be object or array
					if (size == (count - 1))
					{
						if ((type == AmfTypeMarker::Object) || (type == AmfTypeMarker::EcmaArray))
						{
							auto object = property->GetObject();

							if (object == nullptr)
							{
								logtd("Property is not object: %s", property->GetString().CStr());
								break;
							}

							auto key = trigger_list.at(size);

							{
								auto property_pair = object->GetPair(key, AmfTypeMarker::String);

								if (property_pair != nullptr)
								{
									found = true;
									auto value = property_pair->property.GetString();
									GenerateEvent(event, value);
								}
							}
						}
						else if (property->GetType() == AmfTypeMarker::String)
						{
							found = true;
							auto value = property->GetString();
							GenerateEvent(event, value);
						}
						else
						{
							logtd("Document property type mismatch at %d: %s", size - 1, property->GetString().CStr());
							break;
						}
					}
					else
					{
						if (trigger_list.at(size) != property->GetString())
						{
							logtd("Document property mismatch at %d: %s != %s", size - 1, trigger_list.at(size).CStr(), property->GetString().CStr());

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

			SendDataFrame(pts, cmn::BitstreamFormat::ID3v2, packet_type, tag.Serialize(), false);
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

	bool RtmpStream::SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, const ov::Data *data)
	{
		auto export_data = _export_chunk->ExportStreamData(message_header, data->GetDataAs<uint8_t>(), data->GetLength());

		return SendData(export_data->data(), export_data->size());
	}

	bool RtmpStream::SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data)
	{
		auto export_data = _export_chunk->ExportStreamData(message_header, data->data(), data->size());

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
		if (message_header == nullptr)
		{
			return false;
		}

		ov::ByteStream stream(2048);
		if (document.Encode(stream) == false)
		{
			return false;
		}

		auto data = stream.GetData();
		message_header->body_size = data->GetLength();

		return SendMessagePacket(message_header, data);
	}

	bool RtmpStream::SendAmfConnectResult(uint32_t chunk_stream_id, double transaction_id, double object_encoding)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 0,
																	 0);
		AmfDocument document;

		// _result
		document.AppendProperty(RTMP_ACK_NAME_RESULT);
		document.AppendProperty(transaction_id);

		// properties
		{
			AmfObject object;
			object.Append("fmsVer", "FMS/3,5,2,654");
			object.Append("capabilities", 31.0);
			object.Append("mode", 1.0);
			document.AppendProperty(object);
		}

		// information
		{
			AmfObject object;
			object.Append("level", "status");
			object.Append("code", "NetConnection.Connect.Success");
			object.Append("description", "Connection succeeded.");
			object.Append("clientid", _client_id);
			object.Append("objectEncoding", object_encoding);

			AmfEcmaArray array;
			array.Append("version", "3,5,2,654");
			object.Append("data", array);

			document.AppendProperty(object);
		}

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

		document.AppendProperty(RTMP_CMD_NAME_ONFCPUBLISH);
		document.AppendProperty(0.0);
		document.AppendProperty(AmfProperty::NullProperty());

		{
			AmfObject object;
			object.Append("level", "status");
			object.Append("code", "NetStream.Publish.Start");
			object.Append("description", "FCPublish");
			object.Append("clientid", client_id);
			document.AppendProperty(object);
		}

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

		document.AppendProperty(RTMP_ACK_NAME_RESULT);
		document.AppendProperty(transaction_id);
		document.AppendProperty(AmfProperty::NullProperty());
		document.AppendProperty(static_cast<double>(_rtmp_stream_id));

		return SendAmfCommand(message_header, document);
	}

	bool RtmpStream::SendAmfOnStatus(uint32_t chunk_stream_id,
									 uint32_t stream_id,
									 const char *level,
									 const char *code,
									 const char *description,
									 double client_id)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id,
																	 0,
																	 RtmpMessageTypeID::AMF0_COMMAND,
																	 stream_id,
																	 0);
		AmfDocument document;

		document.AppendProperty(RTMP_CMD_NAME_ONSTATUS);
		document.AppendProperty(0.0);
		document.AppendProperty(AmfProperty::NullProperty());

		{
			AmfObject object;
			object.Append("level", level);
			object.Append("code", code);
			object.Append("description", description);
			object.Append("clientid", client_id);
			document.AppendProperty(object);
		}

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
