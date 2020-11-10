//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"
#include "base/info/application.h"
#include "base/provider/push_provider/application.h"
#include "rtmp_provider_private.h"

#include <orchestrator/orchestrator.h>
#include <base/mediarouter/media_type.h>
#include <base/info/media_extradata.h>

#include <modules/containers/flv/flv_parser.h>
#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/aac/aac_specific_config.h>

/*
Process of publishing 

- Handshakeing
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

namespace pvd
{
	std::shared_ptr<RtmpStream> RtmpStream::Create(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<ov::Socket> &client_socket, const std::shared_ptr<PushProvider> &provider)
	{
		auto stream = std::make_shared<RtmpStream>(source_type, client_id, client_socket, provider);
		if(stream != nullptr)
		{
			stream->Start();
		}
		return stream;
	}

	RtmpStream::RtmpStream(StreamSourceType source_type, uint32_t client_id, std::shared_ptr<ov::Socket> client_socket, const std::shared_ptr<PushProvider> &provider)
		: PushStream(source_type, client_id, provider),

		_vhost_app_name(info::VHostAppName::InvalidVHostAppName())
	{
		_remote = client_socket;
		_import_chunk = std::make_shared<RtmpImportChunk>(RTMP_DEFAULT_CHUNK_SIZE);
		_export_chunk = std::make_shared<RtmpExportChunk>(false, RTMP_DEFAULT_CHUNK_SIZE);
		_media_info = std::make_shared<RtmpMediaInfo>();

		// For debug statistics
		_stream_check_time = time(nullptr);
	}

	RtmpStream::~RtmpStream()
	{
	}

	bool RtmpStream::Start()
	{
		_state = Stream::State::PLAYING;
		return PushStream::Start();
	}

	bool RtmpStream::Stop()
	{
		if(GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		_state = Stream::State::STOPPED;

		if(_remote->GetState() == ov::SocketState::Connected)
		{
			_remote->Close();
		}

		return PushStream::Stop();
	}

	bool RtmpStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		if(GetState() == Stream::State::ERROR || GetState() == Stream::State::STOPPED || GetState() == Stream::State::STOPPING)
		{
			return false;
		}

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

		while(true)
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
			else if(process_size == 0)
			{
				// Need more data
				// logtd("Not enough data");
				break;
			}

			_remained_data = _remained_data->Subdata(process_size);
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

			// app 설정
			if ((index = object->FindName("app")) >= 0 && object->GetType(index) == AmfDataType::String)
			{
				_app_name = object->GetString(index);
			}

			// app 설정
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

				auto app_info = ocst::Orchestrator::GetInstance()->GetApplicationInfo(_vhost_app_name);
				if (app_info.IsValid())
				{
					_app_id = app_info.GetId();
				}
				else
				{
					logte("%s application does not exist", _vhost_app_name.CStr());
					Stop();
					return;
				}
			}
			else
			{
				logtw("Could not obtain tcUrl from the RTMP stream: [%s]", _app_name);

				// TODO(dimiden): If tcUrl is not provided, it's not possible to determine which VHost the request was received,
				// so it does not work properly.
				// So, if there is currently one VHost associated with the RTMP Provider, we need to modifiy it to work without tcUrl.
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

		if (!SendStreamBegin())
		{
			logte("SendStreamBegin Fail");
			return;
		}

		if (!SendAmfConnectResult(header->basic_header.stream_id, transaction_id, object_encoding))
		{
			logte("SendAmfConnectResult Fail");
			return;
		}
	}

	void RtmpStream::OnAmfCreateStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (!SendAmfCreateStreamResult(header->basic_header.stream_id, transaction_id))
		{
			logte("SendAmfCreateStreamResult Fail");
			return;
		}
	}

	bool RtmpStream::CheckSignedPolicy()
	{
		// Check SignedPolicy
		
		auto result = HandleSignedPolicy(_url, _remote->GetLocalAddress(), _signed_policy);
		if(result == CheckSignatureResult::Off)
		{
			return true;
		}
		else if(result == CheckSignatureResult::Pass)
		{
			return true;
		}
		else if(result == CheckSignatureResult::Error)
		{
			logtw("SingedPolicy error : %s", _url->ToUrlString().CStr());
			Stop();
			return false;
		}
		else if(result == CheckSignatureResult::Fail)
		{
			logtw("%s", _signed_policy->GetErrMessage().CStr());
			Stop();
			return false;
		}

		return false;
	}

	void RtmpStream::OnAmfFCPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty() && document.GetProperty(3) != nullptr &&
			document.GetProperty(3)->GetType() == AmfDataType::String)
		{
			// TODO: check if the chunk stream id is already exist, and generates new rtmp_stream_id and client_id.
			if (!SendAmfOnFCPublish(header->basic_header.stream_id, _rtmp_stream_id, _client_id))
			{
				logte("SendAmfOnFCPublish Fail");
				return;
			}
			
			_full_url.Format("%s/%s", _tc_url.CStr(), document.GetProperty(3)->GetString());
			
			_url = ov::Url::Parse(_full_url);
			// PORT can be omitted if port is rtmp default port(1935), but SignedPolicy requires this information.
			if(_url->Port() == 0)
			{
				_url->SetPort(_remote->GetLocalAddress()->Port());
			}

			_stream_name = _url->Stream();
			_import_chunk->SetStreamName(_stream_name);

			CheckSignedPolicy();
		}
	}

	void RtmpStream::OnAmfPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id)
	{
		if (_stream_name.IsEmpty())
		{
			if (document.GetProperty(3) != nullptr && document.GetProperty(3)->GetType() == AmfDataType::String)
			{
				_full_url.Format("%s/%s", _tc_url.CStr(), document.GetProperty(3)->GetString());
			
				_url = ov::Url::Parse(_full_url);
				// PORT can be omitted (1935), but SignedPolicy requires this information.
				if(_url->Port() == 0)
				{
					_url->SetPort(_remote->GetLocalAddress()->Port());
				}

				_stream_name = _url->Stream();
				_import_chunk->SetStreamName(_stream_name);

				CheckSignedPolicy();
			}
			else
			{
				logte("OnPublish - Publish Name None");

				//Reject
				SendAmfOnStatus(header->basic_header.stream_id,
								_rtmp_stream_id,
								(char *)"error",
								(char *)"NetStream.Publish.Rejected",
								(char *)"Authentication Failed.", _client_id);

				return;
			}
		}

		_chunk_stream_id = header->basic_header.stream_id;

		// stream begin 전송
		if (!SendStreamBegin())
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

	bool RtmpStream::OnAmfMetaData(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, int32_t object_index)
	{
		RtmpCodecType video_codec_type = RtmpCodecType::Unknown;
		RtmpCodecType audio_codec_type = RtmpCodecType::Unknown;
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
			_device_string = object->GetString(index);  //DeviceType - XSplit
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
			if (object->GetType(index) == AmfDataType::String && strcmp("mp4a", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::AAC;  //AAC
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("mp3", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::MP3;  //MP3
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp(".mp3", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::MP3;  //MP3
			}
			else if (object->GetType(index) == AmfDataType::String && strcmp("speex", object->GetString(index)) == 0)
			{
				audio_codec_type = RtmpCodecType::SPEEX;  //Speex
			}
			else if (object->GetType(index) == AmfDataType::Number && object->GetNumber(index) == 10.0)
			{
				audio_codec_type = RtmpCodecType::AAC;  //AAC
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

		// Audio bitreate
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

		if(video_codec_type == RtmpCodecType::H264 && audio_codec_type == RtmpCodecType::AAC)
		{
			
		}
		else
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
		int32_t import_size = 0;
		std::shared_ptr<const ov::Data> current_data = data;

		while (current_data->IsEmpty() == false)
		{
			bool is_completed = false;

			import_size = _import_chunk->Import(current_data, &is_completed);

			if (import_size == 0)
			{
				// Need more data
				break;
			}
			else if (import_size < 0)
			{
				logte("An error occurred while parse RTMP data: %d", import_size);
				return import_size;
			}

			if (is_completed)
			{
				if (ReceiveChunkMessage() == false)
				{
					logtd("ReceiveChunkMessage Fail");
					logtp("Failed to import packet\n%s", current_data->Dump(current_data->GetLength()).CStr());

					return -1LL;
				}
			}

			logtp("Imported\n%s", current_data->Dump(import_size).CStr());

			process_size += import_size;
			current_data = current_data->Subdata(import_size);
		}

		// Accumulate processed bytes for acknowledgement
		_acknowledgement_traffic += process_size;

		if (_acknowledgement_traffic > _acknowledgement_size)
		{
			SendAcknowledgementSize(_acknowledgement_traffic);

			// Init
			_acknowledgement_traffic = 0;
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

			bool bSuccess = true;

			switch (message->header->completed.type_id)
			{
				case RTMP_MSGID_AUDIO_MESSAGE:
					bSuccess = ReceiveAudioMessage(message);
					break;
				case RTMP_MSGID_VIDEO_MESSAGE:
					bSuccess = ReceiveVideoMessage(message);
					break;
				case RTMP_MSGID_SET_CHUNK_SIZE:
					bSuccess = ReceiveSetChunkSize(message);
					break;
				case RTMP_MSGID_AMF0_DATA_MESSAGE:
					ReceiveAmfDataMessage(message);
					break;
				case RTMP_MSGID_AMF0_COMMAND_MESSAGE:
					ReceiveAmfCommandMessage(message);
					break;
				case RTMP_MSGID_WINDOWACKNOWLEDGEMENT_SIZE:
					ReceiveWindowAcknowledgementSize(message);
					break;
				default:
					logtw("Unknown Type - Type(%d)", message->header->completed.type_id);
					break;
			}

			if (!bSuccess)
			{
				return false;
			}
		}

		return true;
	}

	bool RtmpStream::ReceiveSetChunkSize(const std::shared_ptr<const RtmpMessage> &message)
	{
		auto chunk_size = RtmpMuxUtil::ReadInt32(message->payload->GetData());

		if (chunk_size <= 0)
		{
			logte("ChunkSize Fail - Size(%d) ***", chunk_size);
			return false;
		}

		_import_chunk->SetChunkSize(chunk_size);
		logtd("Set Receive ChunkSize(%u)", chunk_size);

		return true;
	}

	void RtmpStream::ReceiveWindowAcknowledgementSize(const std::shared_ptr<const RtmpMessage> &message)
	{
		auto ackledgement_size = RtmpMuxUtil::ReadInt32(message->payload->GetData());

		if (ackledgement_size != 0)
		{
			_acknowledgement_size = ackledgement_size / 2;
			_acknowledgement_traffic = 0;
		}
	}

	void RtmpStream::ReceiveAmfCommandMessage(const std::shared_ptr<const RtmpMessage> &message)
	{
		AmfDocument document;
		ov::String message_name;
		double transaction_id = 0.0;

		OV_ASSERT2(message->header != nullptr);
		OV_ASSERT2(message->payload != nullptr);

		if (document.Decode(message->payload->GetData(), message->header->payload_size) == 0)
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
			;
		}
		else if (message_name == RTMP_PING)
		{
			;
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
		int32_t decode_lehgth = 0;
		ov::String message_name;
		ov::String data_name;

		decode_lehgth = document.Decode(message->payload->GetData(), message->header->payload_size);
		if (decode_lehgth == 0)
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
		else
		{
			logtw("Unknown Amf0DataMessage - Message(%s)", message_name.CStr());
			return;
		}
	}

	bool RtmpStream::CheckReadyToPublish()
	{
		if(_media_info->video_stream_coming == true && _media_info->audio_stream_coming == true)
		{
			return true;
		}
		// If this algorithm causes latency, it may be better to use meta data from AmfMeta. 
		// But AmfMeta is not always correct so we need more consideration
		
		// It makes this stream video only
		else if(_media_info->video_stream_coming == true && _stream_message_cache_video_count > MAX_STREAM_MESSAGE_COUNT/2)
		{
			return true;
		}
		// It makes this stream audio only
		else if(_media_info->audio_stream_coming == true && _stream_message_cache_audio_count > MAX_STREAM_MESSAGE_COUNT/2)
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
		// size check
		if ((message->header->payload_size < RTMP_VIDEO_DATA_MIN_SIZE) ||
			(message->header->payload_size > RTMP_MAX_PACKET_SIZE))
		{
			logte("Size Fail - stream(%s/%s) size(%d)",
				_vhost_app_name.CStr(), _stream_name.CStr(),
				message->header->payload_size);
			return false;
		}

		const std::shared_ptr<const ov::Data> &payload = message->payload;

		if (!IsPublished())
		{
			_media_info->video_stream_coming = true;

			if(CheckReadyToPublish() == true)
			{
				if(PublishStream() == false)
				{
					logte("Input create fail -  stream(%s/%s)", _vhost_app_name.CStr(), _stream_name.CStr());
					return false;
				}
			}
			else
			{
				// Store stream data until stream is published
				_stream_message_cache.push_back(message);
				_stream_message_cache_video_count ++;
				if (_stream_message_cache.size() > MAX_STREAM_MESSAGE_COUNT)
				{
					logtw("Rtmp input stream init meessage count over -  stream(%s/%s) size(%d:%d)",
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
			if(FlvVideoData::Parse(message->payload->GetDataAs<uint8_t>(), message->payload->GetLength(), flv_video) == false)
			{
				logte("Could not parse flv video (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			int64_t dts = message->header->completed.timestamp;
			int64_t pts = dts + flv_video.CompositionTime();

			auto video_track = GetTrack(RTMP_VIDEO_TRACK_ID);
			if(video_track == nullptr)
			{
				logte("Cannot get video track (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			dts *= video_track->GetVideoTimestampScale();
			pts *= video_track->GetVideoTimestampScale();

			cmn::PacketType	packet_type = cmn::PacketType::Unknwon;
			if(flv_video.PacketType() == FlvAvcPacketType::AVC_SEQUENCE_HEADER)
			{
				packet_type = cmn::PacketType::SEQUENCE_HEADER;

				// AVCDecoderConfigurationRecord Unit Test
				AVCDecoderConfigurationRecord record;
				AVCDecoderConfigurationRecord::Parse(flv_video.Payload(), flv_video.PayloadLength(), record);
			}
			else if(flv_video.PacketType() == FlvAvcPacketType::AVC_NALU)
			{
				packet_type = cmn::PacketType::NALU;
			}
			else if(flv_video.PacketType() == FlvAvcPacketType::AVC_END_SEQUENCE)
			{
				// what can I do?
				return true;
			}

			auto data = std::make_shared<ov::Data>(flv_video.Payload(), flv_video.PayloadLength());
			auto video_frame = std::make_shared<MediaPacket>(cmn::MediaType::Video,
											  RTMP_VIDEO_TRACK_ID,
											  data,
											  pts,
											  dts, 
											  cmn::BitstreamFormat::H264_AVCC, // RTMP's packet type is AVCC 
											 packet_type);

			SendFrame(video_frame);

			// Statistics for debugging
			if (flv_video.FrameType() == FlvVideoFrameTypes::KEY_FRAME)
			{
				_key_frame_interval = message->header->completed.timestamp - _previous_key_frame_timestamp;
				_previous_key_frame_timestamp = message->header->completed.timestamp;
			}

			_last_video_timestamp = message->header->completed.timestamp;
			_video_frame_count++;

			time_t current_time = time(nullptr);
			uint32_t check_gap = current_time - _stream_check_time;

			if (check_gap >= 60)
			{
				logtd("Rtmp Provider Info - stream(%s/%s) key(%ums) timestamp(v:%ums/a:%ums/g:%dms) fps(v:%u/a:%u) gap(v:%ums/a:%ums)",
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
		// size check
		if ((message->header->payload_size < RTMP_AAC_AUDIO_DATA_MIN_SIZE) ||
			(message->header->payload_size > RTMP_MAX_PACKET_SIZE))
		{
			logte("Size Fail - stream(%s/%s) size(%d)",
				_vhost_app_name.CStr(),
				_stream_name.CStr(),
				message->header->payload_size);

			return false;
		}

		if (!IsPublished())
		{
			_media_info->audio_stream_coming = true;

			if(CheckReadyToPublish() == true)
			{
				if(PublishStream() == false)
				{
					logte("Input create fail -  stream(%s/%s)", _vhost_app_name.CStr(), _stream_name.CStr());
					return false;
				}
			}
			else
			{
				// Store stream data until stream is published
				_stream_message_cache.push_back(message);
				_stream_message_cache_video_count ++;
				if (_stream_message_cache.size() > MAX_STREAM_MESSAGE_COUNT)
				{
					logtw("Rtmp input stream init meessage count over -  stream(%s/%s) size(%d:%d)",
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
			if(FlvAudioData::Parse(message->payload->GetDataAs<uint8_t>(), message->payload->GetLength(), flv_audio) == false)
			{
				logte("Could not parse flv audio (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			int64_t pts = message->header->completed.timestamp;
			int64_t dts = pts;
			
			// Get audio track info
			auto audio_track = GetTrack(RTMP_AUDIO_TRACK_ID);
			if(audio_track == nullptr)
			{
				logte("Cannot get video track (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
				return false;
			}

			pts *= audio_track->GetAudioTimestampScale();
			dts *= audio_track->GetAudioTimestampScale();

			cmn::PacketType	packet_type = cmn::PacketType::Unknwon;
			if(flv_audio.PacketType() == FlvAACPacketType::SEQUENCE_HEADER)
			{
				packet_type = cmn::PacketType::SEQUENCE_HEADER;
				// AACSpecificConfig Unit Test
				AACSpecificConfig config;
				AACSpecificConfig::Parse(flv_audio.Payload(), flv_audio.PayloadLength(), config);
			}
			else if(flv_audio.PacketType() == FlvAACPacketType::RAW)
			{
				packet_type = cmn::PacketType::RAW;
			}

			auto data = std::make_shared<ov::Data>(flv_audio.Payload(), flv_audio.PayloadLength());
			auto frame = std::make_shared<MediaPacket>(cmn::MediaType::Audio,
											  RTMP_AUDIO_TRACK_ID,
											  data,
											  pts,
											  dts,
											  cmn::BitstreamFormat::AAC_LATM,
											  packet_type);

			SendFrame(frame);											

			_last_audio_timestamp = message->header->completed.timestamp;
			_audio_frame_count++;
		}

		return true;
	}

	bool RtmpStream::PublishStream()
	{
		if(_stream_name.IsEmpty())
		{
			return false;
		}

		SetName(_stream_name);

		// Set Track Info
		SetTrackInfo(_media_info);

		// Publish
		if(PublishInterleavedChannel(_vhost_app_name) == false)
		{
			Stop();
			return false;
		}

		// Send stored messages
		for(auto message : _stream_message_cache)
		{
			if(message->header->completed.type_id == RTMP_MSGID_VIDEO_MESSAGE && _media_info->video_stream_coming)
			{
				ReceiveVideoMessage(message);
			}
			else if(message->header->completed.type_id == RTMP_MSGID_AUDIO_MESSAGE && _media_info->audio_stream_coming)
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
			new_track->SetTimeBase(1, 1000);
			new_track->SetVideoTimestampScale(1.0);

			// Below items are not mandatory, it will be parsed again from SPS parser
			new_track->SetWidth((uint32_t)media_info->video_width);
			new_track->SetHeight((uint32_t)media_info->video_height);
			new_track->SetFrameRate(media_info->video_framerate);

			// Kbps -> bps, it is just metadata
			new_track->SetBitrate(media_info->video_bitrate * 1000);
			
			AddTrack(new_track);
		}

		if (media_info->audio_stream_coming)
		{
			auto new_track = std::make_shared<MediaTrack>();

			new_track->SetId(RTMP_AUDIO_TRACK_ID);
			new_track->SetMediaType(cmn::MediaType::Audio);
			new_track->SetCodecId(cmn::MediaCodecId::Aac);
			new_track->SetTimeBase(1, 1000);
			new_track->SetAudioTimestampScale(1.0);

			//////////////////
			// Below items are not mandatory, it will be parsed again from ADTS parser
			//////////////////
			new_track->SetSampleRate(media_info->audio_samplerate);
			new_track->GetSample().SetFormat(cmn::AudioSample::Format::S16);
			// Kbps -> bps
			new_track->SetBitrate(media_info->audio_bitrate * 1000);
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

		return true;
	}

	bool RtmpStream::SendData(int data_size, uint8_t *data)
	{
		int remained = data_size;
		uint8_t *data_to_send = data;

		while (remained > 0L)
		{
			int to_send = std::min(remained, (int)(1024L * 1024L));
			int sent = _remote->Send(data_to_send, to_send);

			if (sent != to_send)
			{
				logtw("Send Data Loop Fail");
				return false;
			}

			remained -= sent;
			data_to_send += sent;
		}

		return true;
	}

	bool RtmpStream::SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data)
	{
		if (message_header == nullptr)
		{
			return false;
		}

		auto export_data = _export_chunk->ExportStreamData(message_header, data);

		if (export_data == nullptr || export_data->data() == nullptr)
		{
			return false;
		}

		return SendData(export_data->size(), export_data->data());
	}

		//====================================================================================================
	// Send handshake packet
	// s0 + s1 + s2
	//====================================================================================================
	bool RtmpStream::SendHandshake(const std::shared_ptr<const ov::Data> &data)
	{
		uint8_t s0 = 0;
		uint8_t s1[RTMP_HANDSHAKE_PACKET_SIZE] = {0,};
		uint8_t s2[RTMP_HANDSHAKE_PACKET_SIZE] = {0,};

		s0 = RTMP_HANDSHAKE_VERSION;
		RtmpHandshake::MakeS1(s1);
		RtmpHandshake::MakeS2(data->GetDataAs<uint8_t>() + sizeof(uint8_t), s2);
		_handshake_state = RtmpHandshakeState::C1;

		// Send s0
		if (SendData(sizeof(s0), &s0) == false)
		{
			logte("Handshake s0 Send Fail");
			return false;
		}
		_handshake_state = RtmpHandshakeState::S0;

		// Send s1
		if (SendData(sizeof(s1), s1) == false)
		{
			logte("Handshake s1 Send Fail");
			return false;
		}
		_handshake_state = RtmpHandshakeState::S1;

		// Send s2
		if (SendData(sizeof(s2), s2) == false)
		{
			logte("Handshake s2 Send Fail");
			return false;
		}
		_handshake_state = RtmpHandshakeState::S2;

		return true;
	}

	bool RtmpStream::SendUserControlMessage(uint16_t message, std::shared_ptr<std::vector<uint8_t>> &data)
	{
		auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT,
																	0,
																	RTMP_MSGID_USER_CONTROL_MESSAGE,
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
																	RTMP_MSGID_WINDOWACKNOWLEDGEMENT_SIZE,
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
																	RTMP_MSGID_ACKNOWLEDGEMENT,
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
																	RTMP_MSGID_SET_PEERBANDWIDTH,
																	_rtmp_stream_id,
																	body->size());

		RtmpMuxUtil::WriteInt32(body->data(), bandwidth);
		RtmpMuxUtil::WriteInt8(body->data() + 4, 2);

		return SendMessagePacket(message_header, body);
	}

	bool RtmpStream::SendStreamBegin()
	{
		auto body = std::make_shared<std::vector<uint8_t>>(4);

		RtmpMuxUtil::WriteInt32(body->data(), _rtmp_stream_id);

		return SendUserControlMessage(RTMP_UCMID_STREAMBEGIN, body);
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
																	RTMP_MSGID_AMF0_COMMAND_MESSAGE,
																	_rtmp_stream_id,
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
																	RTMP_MSGID_AMF0_COMMAND_MESSAGE,
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
																	RTMP_MSGID_AMF0_COMMAND_MESSAGE,
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
																	RTMP_MSGID_AMF0_COMMAND_MESSAGE,
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
}