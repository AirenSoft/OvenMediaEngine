//
// Created by getroot on 19. 12. 9.
//


#include "ovt_stream.h"

#define OV_LOG_TAG "OvtStream"

std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<pvd::Application> &app, const std::shared_ptr<ov::Url> &url)
{
	StreamInfo stream_info;

	stream_info.SetId(app->IssueUniqueStreamId());
	stream_info.SetName(url->Stream());

	auto stream = std::make_shared<OvtStream>(app, stream_info, url);
	if(!stream->Start())
	{
		// Explicit deletion
		stream.reset();
		return nullptr;
	}

	return stream;
}

OvtStream::OvtStream(const std::shared_ptr<pvd::Application> &app, const StreamInfo &stream_info, const std::shared_ptr<ov::Url> &url)
	: pvd::Stream(stream_info)
{
	_app = app;
	_url = url;
	_last_request_id = 0;
}

OvtStream::~OvtStream()
{

}

bool OvtStream::Start()
{
	if(!_stop_thread_flag)
	{
		return false;
	}

	if(!ConnectOrigin())
	{
		return false;
	}

	if(!RequestDescribe())
	{
		return false;
	}

	if(!RequestPlay())
	{
		return false;
	}

	_stop_thread_flag = false;
	_worker_thread = std::thread(&OvtStream::WorkerThread, this);

	return true;
}

bool OvtStream::Stop()
{
	_stop_thread_flag = true;
	RequestStop();
	_client_socket.Close();

	return true;
}

bool OvtStream::ConnectOrigin()
{
	ov::SocketType socket_type;

	_url->Scheme().MakeUpper();
	if(_url->Scheme() == "OVT")
	{
		logte("The scheme is not OVT : %s", _url->Scheme().CStr());
		return false;
	}


	if(!_client_socket.Create(ov::SocketType::Tcp))
	{
		logte("To create client socket is failed.");
		return false;
	}

	ov::SocketAddress socket_address(_url->Domain(), _url->Port());

	auto error = _client_socket.Connect(socket_address, 1000);
	if(error != nullptr)
	{
		logte("Cannot connect to origin server : %s:%s", _url->Domain().CStr(), _url->Port());
		return false;
	}

	return true;
}

bool OvtStream::RequestDescribe()
{
	OvtPacket	packet;

	packet.SetSessionId(0);
	packet.SetPayloadType(OVT_PAYLOAD_TYPE_DESCRIBE);
	packet.SetMarker(0);
	packet.SetTimestampNow();

	Json::Value root;
	root["id"] = _last_request_id ++;
	root["url"] = _url->Source().CStr();

	auto payload = ov::Json::Stringify(root).ToData(false);

	packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

	_client_socket.Send(packet.GetData());

	return ReceiveDescribe(_last_request_id);
}

bool OvtStream::ReceiveDescribe(uint32_t request_id)
{
	auto packet = ReceivePacket();
	if(packet == nullptr || packet->PayloadLength() <= 0)
	{
		return false;
	}

	// Parsing Payload
	ov::String payload((const char *)packet->Payload(), packet->PayloadLength());
	ov::JsonObject object = ov::Json::Parse(payload);

	if(object.IsNull())
	{
		logte("An invalid response : Json format");
		return false;
	}

	Json::Value &json_id = object.GetJsonValue()["id"];
	Json::Value &json_code = object.GetJsonValue()["code"];
	Json::Value &json_message = object.GetJsonValue()["message"];
	Json::Value &json_stream = object.GetJsonValue()["stream"];

	if(!json_id.isUInt() || !json_code.isUInt() || json_message.isNull() || json_stream.isNull())
	{
		logte("An invalid response : There are no required keys");
		return false;
	}

	if(request_id != json_id.asUInt())
	{
		logte("An invalid response : Response ID is wrong.");
		return false;
	}

	if(json_code.asUInt() != 200)
	{
		logte("Server Failure : %s (%d)", json_code.asUInt(), json_message.asString().c_str());
		return false;
	}

	// Parse stream and add track
	auto json_tracks = json_stream["tracks"];

	// Validation
	if(json_stream["appName"].isNull() || json_stream["streamName"].isNull() || json_stream["tracks"].isNull() ||
		!json_tracks.isArray())
	{
		logte("Invalid json payload : stream");
		return false;
	}

	SetName(json_stream["streamName"].asString().c_str());
	std::shared_ptr<MediaTrack>	new_track;

	for(int i=0; i<json_tracks.size(); i++)
	{
		auto json_track = json_tracks[i];

		new_track = std::make_shared<MediaTrack>();

		// Validation
		if(!json_track["id"].isUInt() || !json_track["codecId"].isUInt() || !json_track["mediaType"].isUInt() ||
			!json_track["timebase_num"].isUInt() || !json_track["timebase_den"].isUInt() || !json_track["bitrate"].isUInt() ||
			!json_track["startFrameTime"].isUInt64() || !json_track["lastFrameTime"].isUInt64())
		{
			logte("Invalid json track [%d]", i);
			return false;
		}

		new_track->SetId(json_track["id"].asUInt());
		new_track->SetCodecId(static_cast<common::MediaCodecId>(json_track["codecId"].asUInt()));
		new_track->SetMediaType(static_cast<common::MediaType>(json_track["mediaType"].asUInt()));
		new_track->SetTimeBase(json_track["timebase_num"].asUInt(), json_track["timebase_den"].asUInt());
		new_track->SetBitrate(json_track["bitrate"].asUInt());
		new_track->SetStartFrameTime(json_track["startFrameTime"].asUInt64());
		new_track->SetLastFrameTime(json_track["lastFrameTime"].asUInt64());

		// video or audio
		if(new_track->GetMediaType() == common::MediaType::Video)
		{
			auto json_video_track = json_track["videoTrack"];
			if(json_video_track.isNull())
			{
				logte("Invalid json videoTrack");
				return false;
			}

			new_track->SetFrameRate(json_video_track["framerate"].asDouble());
			new_track->SetWidth(json_video_track["width"].asUInt());
			new_track->SetHeight(json_video_track["height"].asUInt());
		}
		else if(new_track->GetMediaType() == common::MediaType::Audio)
		{
			auto json_audio_track = json_track["audioTrack"];
			if(json_audio_track.isNull())
			{
				logte("Invalid json audioTrack");
				return false;
			}

			new_track->SetSampleRate(json_audio_track["samplerate"].asUInt());
			new_track->GetSample().SetFormat(static_cast<common::AudioSample::Format>(json_audio_track["sampleFormat"].asInt()));
			new_track->GetChannel().SetLayout(static_cast<common::AudioChannel::Layout>(json_audio_track["layout"].asUInt()));
		}

		AddTrack(new_track);
	}

	return true;
}

bool OvtStream::RequestPlay()
{
	OvtPacket	packet;

	packet.SetSessionId(0);
	packet.SetPayloadType(OVT_PAYLOAD_TYPE_PLAY);
	packet.SetMarker(0);
	packet.SetTimestampNow();

	Json::Value root;
	root["id"] = _last_request_id ++;
	root["url"] = _url->Source().CStr();

	auto payload = ov::Json::Stringify(root).ToData(false);

	packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

	_client_socket.Send(packet.GetData());

	return ReceivePlay(_last_request_id);
}

bool OvtStream::ReceivePlay(uint32_t request_id)
{
	auto packet = ReceivePacket();
	if(packet == nullptr || packet->PayloadLength() <= 0)
	{
		return false;
	}

	// Parsing Payload
	ov::String payload((const char *)packet->Payload(), packet->PayloadLength());
	ov::JsonObject object = ov::Json::Parse(payload);

	if(object.IsNull())
	{
		logte("An invalid response : Json format");
		return false;
	}

	Json::Value &json_id = object.GetJsonValue()["id"];
	Json::Value &json_code = object.GetJsonValue()["code"];
	Json::Value &json_message = object.GetJsonValue()["message"];

	if(!json_id.isUInt() || !json_code.isUInt() || json_message.isNull())
	{
		logte("An invalid response : There are no required keys");
		return false;
	}

	if(request_id != json_id.asUInt())
	{
		logte("An invalid response : Response ID is wrong.");
		return false;
	}

	if(json_code.asUInt() != 200)
	{
		logte("Server Failure : %s (%d)", json_code.asUInt(), json_message.asString().c_str());
		return false;
	}

	_session_id = packet->SessionId();

	return true;
}

bool OvtStream::RequestStop()
{
	OvtPacket	packet;

	packet.SetSessionId(_session_id);
	packet.SetPayloadType(OVT_PAYLOAD_TYPE_STOP);
	packet.SetMarker(0);
	packet.SetTimestampNow();

	Json::Value root;
	root["id"] = _last_request_id ++;
	root["url"] = _url->Source().CStr();

	auto payload = ov::Json::Stringify(root).ToData(false);

	packet.SetPayload(payload->GetDataAs<uint8_t>(), payload->GetLength());

	_client_socket.Send(packet.GetData());

	return ReceiveStop(_last_request_id);
}

bool OvtStream::ReceiveStop(uint32_t request_id)
{
	auto packet = ReceivePacket();
	if(packet == nullptr || packet->PayloadLength() <= 0)
	{
		return false;
	}

	// Parsing Payload
	ov::String payload((const char *)packet->Payload(), packet->PayloadLength());
	ov::JsonObject object = ov::Json::Parse(payload);

	if(object.IsNull())
	{
		logte("An invalid response : Json format");
		return false;
	}

	Json::Value &json_id = object.GetJsonValue()["id"];
	Json::Value &json_code = object.GetJsonValue()["code"];
	Json::Value &json_message = object.GetJsonValue()["message"];

	if(!json_id.isUInt() || !json_code.isUInt() || json_message.isNull())
	{
		logte("An invalid response : There are no required keys");
		return false;
	}

	if(request_id != json_id.asUInt())
	{
		logte("An invalid response : Response ID is wrong.");
		return false;
	}

	if(json_code.asUInt() != 200)
	{
		logte("Server Failure : %s (%d)", json_code.asUInt(), json_message.asString().c_str());
		return false;
	}

	return true;
}

std::shared_ptr<OvtPacket> OvtStream::ReceivePacket()
{
	auto packet = std::make_shared<OvtPacket>();
	auto data = ov::Data();
	data.SetLength(OVT_FIXED_HEADER_SIZE);

	char *buffer = data.GetWritableDataAs<char>();

	// Receive Header
	off_t offset = 0LL;
	size_t remained = data.GetLength();
	size_t read_bytes = 0ULL;
	while(true)
	{
		auto error = _client_socket.Recv(buffer + offset, remained, &read_bytes);
		if (error != nullptr)
		{
			logte("An error occurred while receive data: %s", error->ToString().CStr());
			_client_socket.Close();
			return nullptr;
		}

		remained -= read_bytes;
		offset += read_bytes;

		if(remained == 0)
		{
			// Received the header completely
			if(!packet->LoadHeader(data))
			{
				logte("An error occurred while receive data: Invalid packet");
				_client_socket.Close();
				return nullptr;
			}

			break;
		}
		else if(remained > 0)
		{
			continue;
		}
		else if(remained < 0)
		{
			logte("An error occurred while receive data: Socket is not working properly.");
			_client_socket.Close();
			return nullptr;
		}
	}

	if(packet->PayloadLength() == 0)
	{
		return packet;
	}

	// Receive payload
	data.SetLength(packet->PayloadLength());
	buffer = data.GetWritableDataAs<char>();
	offset = 0L;
	remained = packet->PayloadLength();
	read_bytes = 0ULL;

	while(true)
	{
		auto error = _client_socket.Recv(buffer + offset, remained, &read_bytes);
		if (error != nullptr)
		{
			logte("An error occurred while receive data: %s", error->ToString().CStr());
			_client_socket.Close();
			return nullptr;
		}

		remained -= read_bytes;
		offset += read_bytes;

		if(remained == 0)
		{
			// Received the header completely
			if(!packet->SetPayload(data.GetDataAs<uint8_t>(), data.GetLength()))
			{
				logte("An error occurred while receive data: Invalid packet");
				_client_socket.Close();
				return nullptr;
			}

			break;
		}
		else if(remained > 0)
		{
			continue;
		}
		else if(remained < 0)
		{
			logte("An error occurred while receive data: Socket is not working properly.");
			_client_socket.Close();
			return nullptr;
		}
	}
}

void OvtStream::WorkerThread()
{
	while(!_stop_thread_flag)
	{
		auto packet = ReceivePacket();

		// Validation
		if(packet == nullptr || packet->SessionId() != _session_id || packet->PayloadType() != OVT_PAYLOAD_TYPE_MEDIA_PACKET)
		{
			logte("An error occurred while receive data: An unexpected packet was received. Delete stream : %s", GetName().CStr());
			Stop();
			_app->NotifyStreamDeleted(GetSharedPtrAs<pvd::Stream>());
			break;
		}

		_depacketizer.AppendPacket(packet);

		if(_depacketizer.IsAvaliableMediaPacket())
		{
			auto media_packet = _depacketizer.PopMediaPacket();
			_app->SendFrame(GetSharedPtrAs<StreamInfo>(), media_packet);
		}
	}
}