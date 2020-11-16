//
// Created by getroot on 19. 12. 9.
//

#include "base/info/application.h"

#include "ovt_stream.h"

#define OV_LOG_TAG "OvtStream"

namespace pvd
{
	std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<pvd::PullApplication> &application, 
											const uint32_t stream_id, const ov::String &stream_name,
					  						const std::vector<ov::String> &url_list)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::Ovt);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<OvtStream>(application, stream_info, url_list);
		if (!stream->Start())
		{
			// Explicit deletion
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	OvtStream::OvtStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list)
			: pvd::PullStream(application, stream_info)
	{
		_last_request_id = 0;
		_state = State::IDLE;

		for(auto &url : url_list)
		{
			auto parsed_url = ov::Url::Parse(url);
			if(parsed_url)
			{
				_url_list.push_back(parsed_url);
			}
		}

		if(!_url_list.empty())
		{
			_curr_url = _url_list[0];
			SetMediaSource(_curr_url->ToUrlString(true));
		}
	}

	OvtStream::~OvtStream()
	{
		Stop();

		_client_socket.Close();
	}

	bool OvtStream::Start()
	{
		_recv_buffer.SetLength(OVT_DEFAULT_MAX_PACKET_SIZE);
		ResetRecvBuffer();

		// For statistics
		auto begin = std::chrono::steady_clock::now();
		if (!ConnectOrigin())
		{
			return false;
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - begin;
		_origin_request_time_msec = static_cast<int64_t>(elapsed.count());

		begin = std::chrono::steady_clock::now();
		if (!RequestDescribe())
		{
			return false;
		}

		end = std::chrono::steady_clock::now();
		elapsed = end - begin;
		_origin_response_time_msec = static_cast<int64_t>(elapsed.count());

		return pvd::PullStream::Start();
	}
	
	bool OvtStream::Play()
	{
		if (!RequestPlay())
		{
			return false;
		}

		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(GetSharedPtr()));
		if(_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginRequestTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginResponseTimeMSec(_origin_response_time_msec);
		}

		return pvd::PullStream::Play();
	}

	bool OvtStream::Stop()
	{
		// Already stopping
		if(_state != State::PLAYING)
		{
			return true;
		}
		
		if(!RequestStop())
		{
			// Force terminate 
			SetState(State::ERROR);
		}
		else
		{
			SetState(State::STOPPED);
		}
	
		return pvd::PullStream::Stop();
	}

	bool OvtStream::ConnectOrigin()
	{
		if(_state != State::IDLE && _state != State::ERROR)
		{
			return false;
		}

		if(_curr_url == nullptr)
		{
			logte("Origin url is not set");
			return false;
		}

		auto scheme = _curr_url->Scheme();
		if (scheme == "OVT")
		{
			_state = State::ERROR;
			logte("The scheme is not OVT : %s", scheme.CStr());
			return false;
		}
		

		if (!_client_socket.Create(ov::SocketType::Tcp))
		{
			_state = State::ERROR;
			logte("To create client socket is failed.");
			return false;
		}

		ov::SocketAddress socket_address(_curr_url->Host(), _curr_url->Port());

		auto error = _client_socket.Connect(socket_address, 1000);
		if (error != nullptr)
		{
			_state = State::ERROR;
			logte("Cannot connect to origin server (%s) : %s:%d", error->GetMessage().CStr(), _curr_url->Host().CStr(), _curr_url->Port());
			return false;
		}

		_state = State::CONNECTED;

		return true;
	}

	bool OvtStream::RequestDescribe()
	{
		if(_state != State::CONNECTED)
		{
			return false;
		}

		OvtPacket packet;

		packet.SetSessionId(0);
		packet.SetPayloadType(OVT_PAYLOAD_TYPE_DESCRIBE);
		packet.SetMarker(0);
		packet.SetTimestampNow();

		Json::Value root;

		_last_request_id++;
		root["id"] = _last_request_id;
		root["url"] = _curr_url->Source().CStr();

		auto payload = ov::Json::Stringify(root).ToData(false);

		packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

		auto sent_size = _client_socket.Send(packet.GetData());
		if(sent_size != static_cast<ssize_t>(packet.GetData()->GetLength()))
		{
			_state = State::ERROR;
			logte("Could not send Describe message");
			return false;
		}

		return ReceiveDescribe(_last_request_id);
	}

	bool OvtStream::ReceiveDescribe(uint32_t request_id)
	{
		auto data = ReceiveMessage();
		if (data == nullptr || data->GetLength() <= 0)
		{
			_state = State::ERROR;
			return false;
		}

		// Parsing Payload
		ov::String payload = data->GetDataAs<char>();
		ov::JsonObject object = ov::Json::Parse(payload);

		if (object.IsNull())
		{
			_state = State::ERROR;
			logte("An invalid response : Json format");
			return false;
		}

		Json::Value &json_id = object.GetJsonValue()["id"];
		Json::Value &json_code = object.GetJsonValue()["code"];
		Json::Value &json_message = object.GetJsonValue()["message"];
		Json::Value &json_stream = object.GetJsonValue()["stream"];

		if (!json_id.isUInt() || !json_code.isUInt() || json_message.isNull())
		{
			_state = State::ERROR;
			logte("An invalid response : There are no required keys");
			return false;
		}

		if (request_id != json_id.asUInt())
		{
			_state = State::ERROR;
			logte("An invalid response : Response ID is wrong. (%d / %d)", request_id, json_id.asUInt());
			return false;
		}

		if (json_code.asUInt() != 200)
		{
			_state = State::ERROR;
			logte("Describe : Server Failure : %d (%s)", json_code.asUInt(), json_message.asString().c_str());
			return false;
		}

		if (json_stream.isNull())
		{
			_state = State::ERROR;
			logte("An invalid response : There is no stream key");
			return false;
		}

		// Parse stream and add track
		auto json_tracks = json_stream["tracks"];

		// Validation
		if (json_stream["appName"].isNull() || json_stream["streamName"].isNull() || json_stream["tracks"].isNull() ||
			!json_tracks.isArray())
		{
			_state = State::ERROR;
			logte("Invalid json payload : stream");
			return false;
		}

		//SetName(json_stream["streamName"].asString().c_str());
		std::shared_ptr<MediaTrack> new_track;

		for (size_t i = 0; i < json_tracks.size(); i++)
		{
			auto json_track = json_tracks[static_cast<int>(i)];

			new_track = std::make_shared<MediaTrack>();

			// Validation
			if (!json_track["id"].isUInt() || !json_track["codecId"].isUInt() || !json_track["mediaType"].isUInt() ||
				!json_track["timebase_num"].isUInt() || !json_track["timebase_den"].isUInt() ||
				!json_track["bitrate"].isUInt() ||
				!json_track["startFrameTime"].isUInt64() || !json_track["lastFrameTime"].isUInt64())
			{
				_state = State::ERROR;
				logte("Invalid json track [%d]", i);
				return false;
			}

			new_track->SetId(json_track["id"].asUInt());
			new_track->SetCodecId(static_cast<cmn::MediaCodecId>(json_track["codecId"].asUInt()));
			new_track->SetMediaType(static_cast<cmn::MediaType>(json_track["mediaType"].asUInt()));
			new_track->SetTimeBase(json_track["timebase_num"].asUInt(), json_track["timebase_den"].asUInt());
			new_track->SetBitrate(json_track["bitrate"].asUInt());
			new_track->SetStartFrameTime(json_track["startFrameTime"].asUInt64());
			new_track->SetLastFrameTime(json_track["lastFrameTime"].asUInt64());

			// video or audio
			if (new_track->GetMediaType() == cmn::MediaType::Video)
			{
				auto json_video_track = json_track["videoTrack"];
				if (json_video_track.isNull())
				{
					_state = State::ERROR;
					logte("Invalid json videoTrack");
					return false;
				}

				new_track->SetFrameRate(json_video_track["framerate"].asDouble());
				new_track->SetWidth(json_video_track["width"].asUInt());
				new_track->SetHeight(json_video_track["height"].asUInt());
			}
			else if (new_track->GetMediaType() == cmn::MediaType::Audio)
			{
				auto json_audio_track = json_track["audioTrack"];
				if (json_audio_track.isNull())
				{
					_state = State::ERROR;
					logte("Invalid json audioTrack");
					return false;
				}

				new_track->SetSampleRate(json_audio_track["samplerate"].asUInt());
				new_track->GetSample().SetFormat(
						static_cast<cmn::AudioSample::Format>(json_audio_track["sampleFormat"].asInt()));
				new_track->GetChannel().SetLayout(
						static_cast<cmn::AudioChannel::Layout>(json_audio_track["layout"].asUInt()));
			}

			AddTrack(new_track);
		}

		_state = State::DESCRIBED;
		return true;
	}

	bool OvtStream::RequestPlay()
	{
		if(_state != State::DESCRIBED)
		{
			return false;
		}

		OvtPacket packet;

		packet.SetSessionId(0);
		packet.SetPayloadType(OVT_PAYLOAD_TYPE_PLAY);
		packet.SetMarker(0);
		packet.SetTimestampNow();

		Json::Value root;
		_last_request_id++;
		root["id"] = _last_request_id;
		root["url"] = _curr_url->Source().CStr();

		auto payload = ov::Json::Stringify(root).ToData(false);

		packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

		auto sent_size = _client_socket.Send(packet.GetData());
		if(sent_size != static_cast<ssize_t>(packet.GetData()->GetLength()))
		{
			_state = State::ERROR;
			logte("Could not send Play message");
			return false;
		}

		return ReceivePlay(_last_request_id);
	}

	bool OvtStream::ReceivePlay(uint32_t request_id)
	{
		auto result = ProceedToReceivePacket();
		std::shared_ptr<OvtPacket> packet = nullptr;
		if(result == ReceivePacketResult::COMPLETE)
		{
			packet = GetPacket();
			if (packet == nullptr || packet->PayloadLength() <= 0)
			{
				_state = State::ERROR;
				return false;
			}
		}
		else
		{
			logte("%s/%s(%u) - Could not receive packet : err(%d)", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId(), static_cast<uint8_t>(result));
			_state = State::ERROR;
			return false;
		}
		
		// Parsing Payload
		ov::String payload((const char *) packet->Payload(), packet->PayloadLength());
		ov::JsonObject object = ov::Json::Parse(payload);

		if (object.IsNull())
		{
			_state = State::ERROR;
			logte("An invalid response : Json format");
			return false;
		}

		Json::Value &json_id = object.GetJsonValue()["id"];
		Json::Value &json_code = object.GetJsonValue()["code"];
		Json::Value &json_message = object.GetJsonValue()["message"];

		if (!json_id.isUInt() || !json_code.isUInt() || json_message.isNull())
		{
			_state = State::ERROR;
			logte("An invalid response : There are no required keys");
			return false;
		}

		if (request_id != json_id.asUInt())
		{
			_state = State::ERROR;
			logte("An invalid response : Response ID is wrong.");
			return false;
		}

		if (json_code.asUInt() != 200)
		{
			_state = State::ERROR;
			logte("Play : Server Failure : %s (%d)", json_code.asUInt(), json_message.asString().c_str());
			return false;
		}

		_session_id = packet->SessionId();

		_state = State::PLAYING;
		return true;
	}

	bool OvtStream::RequestStop()
	{
		if(_state != State::PLAYING)
		{
			return false;
		}

		OvtPacket packet;

		packet.SetSessionId(_session_id);
		packet.SetPayloadType(OVT_PAYLOAD_TYPE_STOP);
		packet.SetMarker(0);
		packet.SetTimestampNow();

		Json::Value root;
		_last_request_id++;
		root["id"] = _last_request_id;
		root["url"] = _curr_url->Source().CStr();

		auto payload = ov::Json::Stringify(root).ToData(false);

		packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

		auto sent_size = _client_socket.Send(packet.GetData());
		if(sent_size != static_cast<ssize_t>(packet.GetData()->GetLength()))
		{
			_state = State::ERROR;
			logte("Could not send Stop message");
			return false;
		}

		return true; 
	}

	bool OvtStream::ReceiveStop(uint32_t request_id, const std::shared_ptr<OvtPacket> &packet)
	{
		if (packet == nullptr || packet->PayloadLength() <= 0)
		{
			_state = State::ERROR;
			return false;
		}

		// Parsing Payload
		ov::String payload((const char *) packet->Payload(), packet->PayloadLength());
		ov::JsonObject object = ov::Json::Parse(payload);

		if (object.IsNull())
		{
			_state = State::ERROR;
			logte("An invalid response : Json format");
			return false;
		}

		Json::Value &json_id = object.GetJsonValue()["id"];
		Json::Value &json_code = object.GetJsonValue()["code"];
		Json::Value &json_message = object.GetJsonValue()["message"];

		if (!json_id.isUInt() || !json_code.isUInt() || json_message.isNull())
		{
			_state = State::ERROR;
			logte("An invalid response : There are no required keys");
			return false;
		}

		if (request_id != json_id.asUInt())
		{
			_state = State::ERROR;
			logte("An invalid response : Response ID is wrong.");
			return false;
		}

		if (json_code.asUInt() != 200)
		{
			_state = State::ERROR;
			logte("Stop : Server Failure : %d (%s)", json_code.asUInt(), json_message.asString().c_str());
			return false;
		}

		_state = State::STOPPED;
		return true;
	}

	std::shared_ptr<ov::Data> OvtStream::ReceiveMessage()
	{
		auto data = std::make_shared<ov::Data>();

		while(true)
		{
			auto result = ProceedToReceivePacket();
			std::shared_ptr<OvtPacket> packet = nullptr;
			if(result == ReceivePacketResult::COMPLETE)
			{
				packet = GetPacket();
				if (packet == nullptr || packet->PayloadLength() <= 0)
				{
					// Unexpected error
					_state = State::ERROR;
					return nullptr;
				}
			}
			else
			{
				logte("%s/%s(%u) - Could not receive packet : err(%d)", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId(), static_cast<uint8_t>(result));
				_state = State::ERROR;
				return nullptr;
			}
		

			data->Append(packet->Payload(), packet->PayloadLength());

			if(packet->Marker())
			{
				break;
			}
		}

		return data;
	}

	void OvtStream::ResetRecvBuffer()
	{
		_recv_buffer_offset = 0;
	}

	OvtStream::ReceivePacketResult OvtStream::ProceedToReceivePacket(bool non_block)
	{
		if(_packet_mold == nullptr)
		{
			_packet_mold = std::make_shared<OvtPacket>();
		}

		if(_packet_mold->IsPacketAvailable() == true)
		{
			return ReceivePacketResult::ALREADY_COMPLETED;
		}
		
		while(true)
		{
			auto buffer = _recv_buffer.GetWritableDataAs<uint8_t>();
			size_t remained = 0;
			size_t read_bytes = 0ULL;

			// Making header is not completed
			if(_packet_mold->IsHeaderAvailable() == false)
			{
				remained = OVT_FIXED_HEADER_SIZE - _recv_buffer_offset;
			}
			else
			{
				remained = _packet_mold->PayloadLength() - _recv_buffer_offset;
			}
				
			auto error = _client_socket.Recv(buffer + _recv_buffer_offset, remained, &read_bytes, non_block);
			if(read_bytes == 0)
			{
				if (error != nullptr)
				{
					logte("An error occurred while receiving packet: %s", error->ToString().CStr());
					_client_socket.Close();
					_state = State::ERROR;
					ResetRecvBuffer();
					return ReceivePacketResult::ERROR;
				}
				else
				{
					return ReceivePacketResult::IMCOMPLETE;
				}
			}
				
			_recv_buffer_offset += read_bytes;
			remained -= read_bytes;

			if(remained == 0)
			{
				if(_packet_mold->IsHeaderAvailable() == false)
				{
					// Received the header completely
					if (!_packet_mold->LoadHeader(_recv_buffer))
					{
						logte("An error occurred while receiving header: Invalid packet");
						_client_socket.Close();
						_state = State::ERROR;
						ResetRecvBuffer();
						return ReceivePacketResult::ERROR;
					}
				}
				else
				{
					if(!_packet_mold->SetPayload(_recv_buffer.GetDataAs<uint8_t>(), _recv_buffer_offset))
					{
						logte("An error occurred while receiving payload: Invalid packet");
						_client_socket.Close();
						_state = State::ERROR;
						ResetRecvBuffer();
						return ReceivePacketResult::ERROR;
					}
				}

				ResetRecvBuffer();

				if (_packet_mold->IsPacketAvailable())
				{
					return ReceivePacketResult::COMPLETE;
				}
				else
				{
					continue;
				}
			}
			else if(remained > 0)
			{
				continue;
			}
			// error
			else if(remained < 0)
			{
				logte("An error occurred while receive data: Invalid packet");
				_client_socket.Close();
				_state = State::ERROR;
				ResetRecvBuffer();
				return ReceivePacketResult::ERROR;
			}
		}

		return ReceivePacketResult::COMPLETE;
	}

	std::shared_ptr<OvtPacket> OvtStream::GetPacket()
	{
		if(_packet_mold == nullptr || _packet_mold->IsPacketAvailable() == false)
		{
			return nullptr;
		}

		// becasue _packet_mold will be initialized when InitRecvBuffer is called
		ResetRecvBuffer();

		return std::move(_packet_mold);;
	}

	int OvtStream::GetFileDescriptorForDetectingEvent()
	{
		return _client_socket.GetSocket().GetSocket();
	}

	PullStream::ProcessMediaResult OvtStream::ProcessMediaPacket()
	{
		// Non block
		auto result = ProceedToReceivePacket(true);
		std::shared_ptr<OvtPacket> packet = nullptr;
		if(result == ReceivePacketResult::COMPLETE)
		{
			packet = GetPacket();
		}
		else if(result == ReceivePacketResult::IMCOMPLETE)
		{
			return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
		}
		else
		{
			logte("%s/%s(%u) - Could not receive packet : err(%d)", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId(), static_cast<uint8_t>(result));
			_state = State::ERROR;
			return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}
		
		// Validation
		if (packet == nullptr)
		{
			logte("The origin server may have problems. Try to terminate %s/%s(%u) stream", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
			_state = State::ERROR;
			return PullStream::ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		if(packet->PayloadType() == OVT_PAYLOAD_TYPE_STOP)
		{
			ReceiveStop(_last_request_id, packet);
			logti(" %s/%s(%u) OvtStream thread has finished gracefully", 
				GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
			_state = State::STOPPED;
			return PullStream::ProcessMediaResult::PROCESS_MEDIA_FINISH;
		}

		if(packet->SessionId() != _session_id)
		{
			logte("An error occurred while receive data: An unexpected packet was received. Terminate stream thread : %s/%s(%u)", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
			_state = State::ERROR;
			return PullStream::ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		if(packet->PayloadType() == OVT_PAYLOAD_TYPE_MEDIA_PACKET)
		{
			_depacketizer.AppendPacket(packet);

			if (_depacketizer.IsAvaliableMediaPacket())
			{
				auto media_packet = _depacketizer.PopMediaPacket();
				media_packet->SetPacketType(cmn::PacketType::OVT);

// Deprecated. The Generate fragment header roll is changed to MediaRouter.
#if 0
				// Make Header (Fragmentation) if it is H.264
				auto track = GetTrack(media_packet->GetTrackId());
				if(track->GetCodecId() == cmn::MediaCodecId::H264)
				{
					H264FragmentHeader::Parse(media_packet);
				}
				else if(track->GetCodecId() == cmn::MediaCodecId::H265)
				{
					
				}
#endif
				SendFrame(media_packet);
			}
		}
		else
		{
			logte("An error occurred while receive data: An unexpected packet was received. Terminate stream thread : %s/%s(%u)", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
			_state = State::ERROR;
			return PullStream::ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		return PullStream::ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}
}