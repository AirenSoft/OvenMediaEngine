//
// Created by getroot on 19. 12. 9.
//

#include "ovt_stream.h"

#include <modules/bitstream/decoder_configuration_record_parser.h>
#include <modules/ovt_packetizer/ovt_signaling.h>

#include "base/info/application.h"
#include "ovt_provider.h"

#define OV_LOG_TAG "OvtStream"

namespace pvd
{
	std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<pvd::PullApplication> &application,
												 const uint32_t stream_id, const ov::String &stream_name,
												 const std::vector<ov::String> &url_list,
												 const std::shared_ptr<pvd::PullStreamProperties> &properties)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::Ovt);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<OvtStream>(application, stream_info, url_list, properties);
		if (!stream->Start())
		{
			// Explicit deletion
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	OvtStream::OvtStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list, const std::shared_ptr<pvd::PullStreamProperties> &properties)
		: pvd::PullStream(application, stream_info, url_list, properties)
	{
		_last_request_id = 0;
		SetState(State::IDLE);
		logtd("OvtStream Created : %d", GetId());
	}

	OvtStream::~OvtStream()
	{
		Release();
		Stop();
		logtd("OvtStream Terminated : %d", GetId());
	}

	void OvtStream::Release()
	{
		if (_client_socket != nullptr)
		{
			_client_socket->Close();
		}

		_curr_url = nullptr;

		std::lock_guard<std::shared_mutex> mlock(_packetizer_lock);
		if (_packetizer != nullptr)
		{
			_packetizer->Release();
			_packetizer.reset();
		}
	}

	bool OvtStream::StartStream(const std::shared_ptr<const ov::Url> &url)
	{
		// Only start from IDLE, ERROR, STOPPED
		if (!(GetState() == State::IDLE || GetState() == State::ERROR || GetState() == State::STOPPED))
		{
			return true;
		}

		_curr_url = url;

		if (_packetizer == nullptr)
		{
			_packetizer = std::make_shared<OvtPacketizer>(OvtPacketizerInterface::GetSharedPtr());
		}

		ov::StopWatch stop_watch;

		// For statistics
		stop_watch.Start();
		if (!ConnectOrigin())
		{
			SetState(Stream::State::ERROR);
			Release();
			return false;
		}
		_origin_request_time_msec = stop_watch.Elapsed();

		stop_watch.Update();
		if (!RequestDescribe())
		{
			SetState(Stream::State::ERROR);
			Release();
			return false;
		}

		if (!RequestPlay())
		{
			SetState(Stream::State::ERROR);
			return false;
		}
		_origin_response_time_msec = stop_watch.Elapsed();

		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(pvd::Stream::GetSharedPtr()));
		if (_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginConnectionTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginSubscribeTimeMSec(_origin_response_time_msec);
		}

		return true;
	}

	bool OvtStream::RestartStream(const std::shared_ptr<const ov::Url> &url)
	{
		logti("[%s/%s(%u)] stream tries to reconnect to %s", GetApplicationTypeName(), GetName().CStr(), GetId(), url->ToUrlString().CStr());
		if (StartStream(url) == false)
		{
			return false;
		}

		return true;
	}

	bool OvtStream::StopStream()
	{
		if (GetState() == State::STOPPED)
		{
			return true;
		}

		if (!RequestStop())
		{
			// Force terminate
			SetState(State::ERROR);
		}

		Release();

		return true;
	}

	std::shared_ptr<pvd::OvtProvider> OvtStream::GetOvtProvider()
	{
		return std::static_pointer_cast<OvtProvider>(GetApplication()->GetParentProvider());
	}

	bool OvtStream::ConnectOrigin()
	{
		if (GetState() == State::PLAYING || GetState() == State::TERMINATED)
		{
			return false;
		}

		if (_curr_url == nullptr)
		{
			logte("Origin url is not set");
			return false;
		}

		auto scheme = _curr_url->Scheme();
		if (scheme.UpperCaseString() != "OVT")
		{
			SetState(State::ERROR);
			logte("The scheme is not OVT : %s", scheme.CStr());
			return false;
		}

		auto pool = GetOvtProvider()->GetClientSocketPool();

		if (pool == nullptr)
		{
			// Provider is not initialized
			return false;
		}

		auto socket_address = ov::SocketAddress::CreateAndGetFirst(_curr_url->Host(), _curr_url->Port());

		_client_socket = pool->AllocSocket(socket_address.GetFamily());

		if (_client_socket == nullptr)
		{
			SetState(State::ERROR);
			logte("To create client socket is failed.");

			_client_socket = nullptr;
			return false;
		}

		_client_socket->SetSockOpt<int>(IPPROTO_TCP, TCP_NODELAY, 1);
		_client_socket->SetSockOpt<int>(IPPROTO_TCP, TCP_QUICKACK, 1);
		_client_socket->MakeBlocking();

		struct timeval tv = {1, 500000};  // 1.5 sec
		_client_socket->SetRecvTimeout(tv);

		auto error = _client_socket->Connect(socket_address, 1500);
		if (error != nullptr)
		{
			SetState(State::ERROR);
			logte("Cannot connect to origin server (%s) : (%s)", error->GetMessage().CStr(), socket_address.ToString().CStr());
			return false;
		}

		SetState(State::CONNECTED);

		return true;
	}

	bool OvtStream::RequestDescribe()
	{
		if (GetState() != State::CONNECTED)
		{
			return false;
		}

		Json::Value root;

		_last_request_id++;
		root["id"] = _last_request_id;
		root["application"] = "describe";
		root["target"] = _curr_url->Source().CStr();

		auto message = ov::Json::Stringify(root).ToData(false);

		std::shared_lock<std::shared_mutex> lock(_packetizer_lock);
		if (_packetizer->PacketizeMessage(OVT_PAYLOAD_TYPE_MESSAGE_REQUEST, ov::Clock::NowMSec(), message) == false)
		{
			return false;
		}

		return ReceiveDescribe(_last_request_id);
	}

	bool OvtStream::ReceiveDescribe(uint32_t request_id)
	{
		auto data = ReceiveMessage();
		if (data == nullptr || data->GetLength() <= 0)
		{
			SetState(State::ERROR);
			return false;
		}

		// Parsing Payload
		ov::String payload = data->GetDataAs<char>();
		ov::JsonObject object = ov::Json::Parse(payload);

		if (object.IsNull())
		{
			SetState(State::ERROR);
			logte("An invalid response : Json format");
			return false;
		}

		Json::Value &json_id = object.GetJsonValue()["id"];
		Json::Value &json_application = object.GetJsonValue()["application"];
		Json::Value &json_code = object.GetJsonValue()["code"];
		Json::Value &json_message = object.GetJsonValue()["message"];
		Json::Value &json_contents = object.GetJsonValue()["contents"];

		if (!json_id.isUInt() || json_application.isNull() || !json_code.isUInt() || json_message.isNull())
		{
			SetState(State::ERROR);
			logte("An invalid response : There are no required keys");
			return false;
		}

		if (request_id != json_id.asUInt())
		{
			SetState(State::ERROR);
			logte("An invalid response : Response ID is wrong. (%d / %d)", request_id, json_id.asUInt());
			return false;
		}

		if (json_code.asUInt() != 200)
		{
			SetState(State::ERROR);
			logte("Describe : Server Failure : %d (%s)", json_code.asUInt(), json_message.asString().c_str());
			return false;
		}

		ov::String application = json_application.asString().c_str();
		if (application.UpperCaseString() != "DESCRIBE")
		{
			SetState(State::ERROR);
			logte("An invalid response : wrong application : %s", application.CStr());
			return false;
		}

		if (json_contents.isNull())
		{
			SetState(State::ERROR);
			logte("An invalid response : There is no contents");
			return false;
		}

		// Parse stream and add track
		auto json_version = json_contents["version"];
		auto json_stream = json_contents["stream"];
		auto json_tracks = json_stream["tracks"];
		auto json_playlists = json_stream["playlists"];

		// Validation
		if (json_version.isNull() || json_version.isUInt() == false)
		{
			uint32_t ovt_sig_version = json_version.asUInt();
			if (ovt_sig_version != OVT_SIGNALING_VERSION)
			{
				SetState(State::ERROR);
				logte("Invalid OVT version : %X, expected %X", ovt_sig_version, OVT_SIGNALING_VERSION);
				return false;
			}
		}

		// renditions is optional
		if (json_stream["appName"].isNull() || json_stream["streamName"].isNull() || json_stream["tracks"].isNull() ||
			!json_tracks.isArray())
		{
			SetState(State::ERROR);
			logte("Invalid json payload : stream");
			return false;
		}

		// Latest version origin server sends UUID of origin stream
		if (json_stream["originStreamUUID"].isString())
		{
			SetOriginStreamUUID(json_stream["originStreamUUID"].asString().c_str());
		}

		// Renditions

		for (size_t i = 0; i < json_playlists.size(); i++)
		{
			auto json_playlist = json_playlists[static_cast<int>(i)];

			// Validate
			if (json_playlist["name"].isNull() || json_playlist["fileName"].isNull() ||
				!json_playlist["options"].isObject() || !json_playlist["renditions"].isArray())
			{
				SetState(State::ERROR);
				logte("Invalid json payload : playlist");
				return false;
			}

			ov::String playlist_name = json_playlist["name"].asString().c_str();
			ov::String playlist_file_name = json_playlist["fileName"].asString().c_str();

			auto playlist = std::make_shared<info::Playlist>(playlist_name, playlist_file_name, false);

			// Options
			auto json_options = json_playlist["options"];

			if (json_options.isNull() == false)
			{
				// Validate
				if (json_options["webrtcAutoAbr"].isBool())
				{
					playlist->SetWebRtcAutoAbr(json_options["webrtcAutoAbr"].asBool());
				}

				if (json_options["hlsChunklistPathDepth"].isInt())
				{
					playlist->SetHlsChunklistPathDepth(json_options["hlsChunklistPathDepth"].asInt());
				}

				if (json_options["enableTsPackaging"].isBool())
				{
					playlist->EnableTsPackaging(json_options["enableTsPackaging"].asBool());
				}
			}

			for (size_t j = 0; j < json_playlist["renditions"].size(); j++)
			{
				auto json_rendition = json_playlist["renditions"][static_cast<int>(j)];

				// Validate
				if (!json_rendition["name"].isString() || !json_rendition["videoTrackName"].isString() || !json_rendition["audioTrackName"].isString())
				{
					SetState(State::ERROR);
					logte("Invalid json payload : playlist rendition");
					return false;
				}

				ov::String rendition_name = json_rendition["name"].asString().c_str();
				ov::String video_track_name = json_rendition["videoTrackName"].asString().c_str();
				ov::String audio_track_name = json_rendition["audioTrackName"].asString().c_str();

				playlist->AddRendition(std::make_shared<info::Rendition>(rendition_name, video_track_name, audio_track_name));
			}

			AddPlaylist(playlist);
		}

		//SetName(json_stream["streamName"].asString().c_str());
		std::shared_ptr<MediaTrack> new_track;

		for (size_t i = 0; i < json_tracks.size(); i++)
		{
			auto json_track = json_tracks[static_cast<int>(i)];

			new_track = std::make_shared<MediaTrack>();

			// Validation
			if (!json_track["id"].isUInt() || !json_track["name"].isString() || !json_track["codecId"].isUInt() || !json_track["mediaType"].isUInt() ||
				!json_track["timebaseNum"].isUInt() || !json_track["timebaseDen"].isUInt() ||
				!json_track["bitrate"].isUInt() ||
				!json_track["startFrameTime"].isUInt64() || !json_track["lastFrameTime"].isUInt64())
			{
				SetState(State::ERROR);
				logte("Invalid json track [%d]", i);
				return false;
			}

			new_track->SetId(json_track["id"].asUInt());
			new_track->SetVariantName(json_track["name"].asString().c_str());
			new_track->SetCodecId(static_cast<cmn::MediaCodecId>(json_track["codecId"].asUInt()));
			new_track->SetMediaType(static_cast<cmn::MediaType>(json_track["mediaType"].asUInt()));
			new_track->SetTimeBase(json_track["timebaseNum"].asUInt(), json_track["timebaseDen"].asUInt());
			new_track->SetBitrateByConfig(json_track["bitrate"].asUInt());
			new_track->SetStartFrameTime(json_track["startFrameTime"].asUInt64());
			new_track->SetLastFrameTime(json_track["lastFrameTime"].asUInt64());

			// video or audio
			if (new_track->GetMediaType() == cmn::MediaType::Video)
			{
				auto json_video_track = json_track["videoTrack"];
				if (json_video_track.isNull())
				{
					SetState(State::ERROR);
					logte("Invalid json videoTrack");
					return false;
				}

				new_track->SetFrameRateByConfig(json_video_track["framerate"].asDouble());
				new_track->SetWidth(json_video_track["width"].asUInt());
				new_track->SetHeight(json_video_track["height"].asUInt());
			}
			else if (new_track->GetMediaType() == cmn::MediaType::Audio)
			{
				auto json_audio_track = json_track["audioTrack"];
				if (json_audio_track.isNull())
				{
					SetState(State::ERROR);
					logte("Invalid json audioTrack");
					return false;
				}

				new_track->SetSampleRate(json_audio_track["samplerate"].asUInt());
				new_track->GetSample().SetFormat(static_cast<cmn::AudioSample::Format>(json_audio_track["sampleFormat"].asInt()));
				new_track->GetChannel().SetLayout(static_cast<cmn::AudioChannel::Layout>(json_audio_track["layout"].asUInt()));
			}

			auto decoder_config = json_track["decoderConfig"];
			if (decoder_config.isString())
			{
				auto config_data = ov::Base64::Decode(decoder_config.asString().c_str());
				auto decoder_config = DecoderConfigurationRecordParser::Parse(new_track->GetCodecId(), config_data);
				new_track->SetDecoderConfigurationRecord(decoder_config);
			}

			// If there is an existing track with the same ID, just update the value
			UpdateTrack(new_track);
		}

		// logti("[%s/%s(%u)] stream has been described . %s", GetApplicationTypeName(), GetName().CStr(), GetId(), payload.CStr());

		SetState(State::DESCRIBED);
		return true;
	}

	bool OvtStream::RequestPlay()
	{
		if (GetState() != State::DESCRIBED)
		{
			logte("%s/%s(%u) - Could not request to play. Before receiving describe.", GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId());
			return false;
		}

		Json::Value root;
		_last_request_id++;
		root["id"] = _last_request_id;
		root["application"] = "play";
		root["target"] = _curr_url->Source().CStr();

		auto message = ov::Json::Stringify(root).ToData(false);

		std::shared_lock<std::shared_mutex> lock(_packetizer_lock);
		if (_packetizer->PacketizeMessage(OVT_PAYLOAD_TYPE_MESSAGE_REQUEST, ov::Clock::NowMSec(), message) == false)
		{
			logte("%s/%s(%u) - Could not request to play. Socket send error", GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId());
			return false;
		}

		return ReceivePlay(_last_request_id);
	}

	bool OvtStream::ReceivePlay(uint32_t request_id)
	{
		auto message = ReceiveMessage();
		if (message == nullptr)
		{
			logte("%s/%s(%u) - Could not receive message", GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId());
			SetState(State::ERROR);
			return false;
		}

		// Parsing Payload
		ov::String payload(message->GetDataAs<char>(), message->GetLength());
		ov::JsonObject object = ov::Json::Parse(payload);

		if (object.IsNull())
		{
			SetState(State::ERROR);
			logte("An invalid response : Json format");
			return false;
		}

		Json::Value &json_id = object.GetJsonValue()["id"];
		Json::Value &json_app = object.GetJsonValue()["application"];
		Json::Value &json_code = object.GetJsonValue()["code"];
		Json::Value &json_message = object.GetJsonValue()["message"];

		if (!json_id.isUInt() || json_app.isNull() || !json_code.isUInt() || json_message.isNull())
		{
			SetState(State::ERROR);
			logte("An invalid response : There are no required keys");
			return false;
		}

		ov::String application = json_app.asString().c_str();

		if (application.UpperCaseString() != "PLAY")
		{
			SetState(State::ERROR);
			logte("An invalid response : application is wrong (%s).", application.CStr());
			return false;
		}

		if (request_id != json_id.asUInt())
		{
			SetState(State::ERROR);
			logte("An invalid response : Response ID is wrong.");
			return false;
		}

		if (json_code.asUInt() != 200)
		{
			SetState(State::ERROR);
			logte("Play : Server Failure : %d (%s)", json_code.asUInt(), json_message.asString().c_str());
			return false;
		}

		SetState(State::PLAYING);
		return true;
	}

	bool OvtStream::RequestStop()
	{
		if (GetState() != State::PLAYING)
		{
			return false;
		}

		Json::Value root;
		_last_request_id++;
		root["id"] = _last_request_id;
		root["application"] = "stop";
		root["target"] = _curr_url->Source().CStr();

		auto message = ov::Json::Stringify(root).ToData(false);

		std::shared_lock<std::shared_mutex> lock(_packetizer_lock);
		if (_packetizer->PacketizeMessage(OVT_PAYLOAD_TYPE_MESSAGE_REQUEST, ov::Clock::NowMSec(), message) == false)
		{
			return false;
		}

		return true;
	}

	bool OvtStream::OnOvtPacketized(std::shared_ptr<OvtPacket> &packet)
	{
		if (_client_socket->Send(packet->GetData()) == false)
		{
			SetState(State::ERROR);
			logte("Could not send message");
			return false;
		}

		return true;
	}

	std::shared_ptr<ov::Data> OvtStream::ReceiveMessage()
	{
		while (true)
		{
			auto result = ReceivePacket();
			if (result == false)
			{
				logte("%s/%s(%u) - Could not receive packet : err(%d)", GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId(), static_cast<uint8_t>(result));
				SetState(State::ERROR);
				return nullptr;
			}

			if (_depacketizer.IsAvailableMessage())
			{
				auto message = _depacketizer.PopMessage();
				return message;
			}
		}

		return nullptr;
	}

	bool OvtStream::ReceivePacket(bool non_block)
	{
		uint8_t buffer[65535];
		size_t read_bytes = 0ULL;

		auto error = _client_socket->Recv(buffer, 65535, &read_bytes, non_block);
		if (read_bytes == 0)
		{
			if (error != nullptr)
			{
				logte("[%s/%s] An error occurred while receiving packet: %s", GetApplicationName(), GetName().CStr(), error->What());
				_client_socket->Close();
				SetState(State::ERROR);
				return false;
			}
			else
			{
				if (non_block == true)
				{
					// retry later
					return true;
				}
				else
				{
					// timeout
					return false;
				}
			}
		}

		if (_depacketizer.AppendPacket(buffer, read_bytes) == false)
		{
			logte("[%s/%s] An error occurred while parsing packet: Invalid packet", GetApplicationName(), GetName().CStr());
			return false;
		}

		return true;
	}

	int OvtStream::GetFileDescriptorForDetectingEvent()
	{
		return _client_socket->GetNativeHandle();
	}

	PullStream::ProcessMediaResult OvtStream::ProcessMediaPacket()
	{
		// Non block
		auto result = ReceivePacket(true);
		if (result == false)
		{
			logte("%s/%s(%u) - Could not receive packet : err(%d)", GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId(), static_cast<uint8_t>(result));
			SetState(State::ERROR);
			return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		while (true)
		{
			if (_depacketizer.IsAvailableMediaPacket())
			{
				auto media_packet = _depacketizer.PopMediaPacket();

				media_packet->SetMsid(GetMsid());
				media_packet->SetPacketType(cmn::PacketType::OVT);

				int64_t pts = media_packet->GetPts();
				int64_t dts = media_packet->GetDts();

				AdjustTimestampByBase(media_packet->GetTrackId(), pts, dts, std::numeric_limits<int64_t>::max());
				[[maybe_unused]] auto old_pts = media_packet->GetPts();
				[[maybe_unused]] auto old_dts = media_packet->GetDts();
				media_packet->SetPts(pts);
				media_packet->SetDts(dts);

				// After the MSID is changed, the packet is dropped until key frame is received.
				bool drop = false;
				if (_last_msid_map[media_packet->GetTrackId()] != media_packet->GetMsid())
				{
					_last_msid_map[media_packet->GetTrackId()] = media_packet->GetMsid();
					//  Do not anything if the msid is changed
				}

				// When switching streams, the PTS of the packet may become negative due to the start time of the first packet. Packets before the base timestamp are defined as a drop policy.
				if (media_packet->GetPts() < 0)
				{
					drop = true;
				}

				if (drop == false)
				{
					SendFrame(media_packet);
				}

				if (_depacketizer.IsAvailableMediaPacket() || _depacketizer.IsAvailableMessage())
				{
					continue;
				}

				return PullStream::ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
			}
			else if (_depacketizer.IsAvailableMessage())
			{
				auto message = _depacketizer.PopMessage();

				// Parsing Payload
				ov::String payload(message->GetDataAs<char>(), message->GetLength());
				ov::JsonObject object = ov::Json::Parse(payload);

				if (object.IsNull())
				{
					Stop();
					SetState(State::ERROR);
					logte("An invalid response : Json format");
					return PullStream::ProcessMediaResult::PROCESS_MEDIA_FAILURE;
				}

				//Json::Value &json_id = object.GetJsonValue()["id"];
				Json::Value &json_app = object.GetJsonValue()["application"];

				ov::String application = json_app.asString().c_str();

				if (application.UpperCaseString() == "STOP")
				{
					logte("An invalid response : application is wrong (%s).", application.CStr());
					return PullStream::ProcessMediaResult::PROCESS_MEDIA_FINISH;
				}
				else
				{
					logte("An error occurred while receive data: An unexpected packet was received. Terminate stream thread : %s/%s(%u)",
						  GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), GetId());
					SetState(State::ERROR);
					return PullStream::ProcessMediaResult::PROCESS_MEDIA_FAILURE;
				}
			}
			else
			{
				return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
			}
		}

		return PullStream::ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}
}  // namespace pvd
