//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "relay_server.h"

#include "relay_private.h"

#include <media_router/media_router.h>
#include <base/media_route/media_route_application_interface.h>
#include <base/media_route/media_buffer.h>

RelayServer::RelayServer(MediaRouteApplicationInterface *media_route_application, const info::Application &application_info)
	: _media_route_application(media_route_application),
	  _application_info(application_info)
{
	// localhost:9000에 대기
	if(application_info.GetType() == cfg::ApplicationType::Live)
	{
		_server_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Srt, ov::SocketAddress(9000));

		if(_server_port != nullptr)
		{
			_server_port->AddObserver(this);
		}
	}
}

RelayServer::~RelayServer()
{
	if(_server_port != nullptr)
	{
		_server_port->RemoveObserver(this);
	}
}

bool RelayServer::OnCreateStream(std::shared_ptr<StreamInfo> info)
{
	// Nothing to do
	return true;
}

bool RelayServer::OnDeleteStream(std::shared_ptr<StreamInfo> info)
{
	// Notify to relay client
	logtd("Stream is deleted: %d, %s", info->GetId(), info->GetName().CStr());
	Send(info->GetId(), RelayPacket(RelayPacketType::Sync), nullptr);

	return true;
}

bool RelayServer::OnSendVideoFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation)
{
	return true;
}

bool RelayServer::OnSendAudioFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation)
{
	return true;
}

void RelayServer::OnConnected(ov::Socket *remote)
{
	logtd("New RelayClient is connected: %s", remote->ToString().CStr());

	_client_list[remote] = ClientInfo();
}

void RelayServer::OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	logtd("Data received from %s: %zu bytes", remote->ToString().CStr(), data->GetLength());

	RelayPacket packet(data.get());

	switch(packet.GetType())
	{
		case RelayPacketType::RequestSync:
		{
			// Extract application and stream name
			// The payload must consist of "app/stream" when type is RequestOffer
			ov::String payload(reinterpret_cast<const char *>(packet.GetData()), packet.GetDataSize());
			auto tokens = payload.Split("/");

			if(tokens.size() != 2)
			{
				// Invalid request. Disconnect...
				logtd("Invalid request: %s, disconnecting...", payload.CStr());
				remote->Close();
				return;
			}

			ov::String app_name = tokens[0];
			ov::String stream_name = tokens[1];

			if(_application_info.GetName() != app_name)
			{
				// Another relay server will process this request
				return;
			}

			auto streams = _media_route_application->GetStreams();

			for(auto &stream_iter : streams)
			{
				const auto &stream_info = stream_iter.second->GetStreamInfo();

				if(stream_info != nullptr)
				{
					if(stream_info->GetName() == stream_name)
					{
						// response
						logtd("Response sync for %s (%d/%d)...", payload.CStr(), _application_info.GetId(), stream_info->GetId());

						// serialize media track
						ov::String serialize;

						serialize.AppendFormat("%s\n", stream_name.CStr());

						// about 45 bytes per track
						for(auto &track_iter : stream_info->GetTracks())
						{
							MediaTrack *track = track_iter.second.get();

							serialize.AppendFormat(
								// Serialize VideoTrack
								// framerate|width|height
								"%.6f|%d|%d|"
								// Serialize AudioTrack
								// samplerate|format|layout
								"%d|%d|%d|"
								// Serialize MediaTrack
								// track_id|codec_id|media_type|timebase.num|timebase.den|bitrate|start_frame_time|last_frame_time
								"%d|%d|%d|%d|%d|%d|%lld|%lld\n",
								track->GetFrameRate(), track->GetWidth(), track->GetHeight(),
								track->GetSampleRate(), track->GetSample().GetFormat(), track->GetChannel().GetLayout(),
								track->GetId(), track->GetCodecId(), track->GetMediaType(), track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(), track->GetBitrate(), track->GetStartFrameTime(), track->GetLastFrameTime()
							);
						}

						logtd("Sync data for %u/%u\n%s", _application_info.GetId(), stream_info->GetId(), serialize.CStr());

						RelayPacket response(RelayPacketType::Sync);
						Send(remote, stream_info->GetId(), response, serialize.ToData().get());

						auto info_iter = _client_list.find(remote);

						if(info_iter != _client_list.end())
						{
							info_iter->second.stream_id = stream_info->GetId();
						}

						_client_map[stream_info->GetId()].push_back(remote);

						return;
					}
				}
			}

			// no data
			logtd("There is no stream for %s (%u)...", payload.CStr(), _application_info.GetId());
			RelayPacket response(RelayPacketType::Sync);

			remote->Send(&response, sizeof(response));

			break;
		}

		case RelayPacketType::Sync:
		case RelayPacketType::Packet:
			logte("Invalid packet type from client: %d", packet.GetType());
			break;

		default:
			OV_ASSERT2(false);
			logte("Invalid packet type from client: %d", packet.GetType());
			break;
	}
}

void RelayServer::OnDisconnected(ov::Socket *remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	logtd("RelayClient is disconnected: %s (reason: %d)", remote->ToString().CStr(), reason);

	auto info_iter = _client_list.find(remote);

	if(info_iter != _client_list.end())
	{
		auto client_list_iter = _client_map.find(info_iter->second.stream_id);

		if(client_list_iter != _client_map.end())
		{
			auto &client_list = client_list_iter->second;

			for(auto client = client_list.begin(); client != client_list.end(); ++client)
			{
				if(*client == remote)
				{
					logtd("Deleting RelayClient for stream: %s", remote->ToString().CStr());
					client_list.erase(client);
					break;
				}
			}
		}
		else
		{
			// The client disconnected as soon as it connected
			// (does not request any data)
		}

		logtd("Deleting RelayClient from client list: %s", remote->ToString().CStr());
		_client_list.erase(info_iter);
	}
	else
	{
		// not found
		OV_ASSERT2(false);
	}
};

void RelayServer::Send(info::stream_id_t stream_id, const RelayPacket &base_packet, const ov::Data *data)
{
	if(_client_map.size() == 0)
	{
		return;
	}

	auto client_list_iter = _client_map.find(stream_id);

	if(client_list_iter == _client_map.end())
	{
		// There is no client to send
		return;
	}

	uint32_t transaction_id = _transaction_id++;

	RelayPacket packet = base_packet;

	packet.SetApplicationId(_application_info.GetId());
	packet.SetStreamId(stream_id);
	packet.SetTransactionId(transaction_id);

	packet.SetStart(true);

	if(data != nullptr)
	{
		ov::ByteStream stream(data);

		while(stream.Remained() > 0)
		{
			size_t read_bytes = stream.Read(static_cast<uint8_t *>(packet.GetData()), RelayPacketDataSize);
			packet.SetDataSize(static_cast<uint16_t>(read_bytes));

			if(stream.Remained() == 0)
			{
				packet.SetEnd(true);
			}

			for(auto &client : client_list_iter->second)
			{
				client->Send(&packet, sizeof(packet));
			}

			packet.SetStart(false);
		}
	}
	else
	{
		packet.SetEnd(true);

		for(auto &client : client_list_iter->second)
		{
			client->Send(&packet, sizeof(packet));
		}
	}
}

void RelayServer::Send(info::stream_id_t stream_id, const RelayPacket &base_packet, const void *data, uint16_t data_size)
{
	if(_client_map.size() == 0)
	{
		return;
	}

	auto data_to_send = ov::Data(data, data_size, true);

	Send(stream_id, base_packet, &data_to_send);
}

void RelayServer::Send(ov::Socket *socket, info::stream_id_t stream_id, const RelayPacket &base_packet, const ov::Data *data)
{
	uint32_t transaction_id = _transaction_id++;

	RelayPacket packet = base_packet;

	packet.SetApplicationId(_application_info.GetId());
	packet.SetStreamId(stream_id);
	packet.SetTransactionId(transaction_id);

	packet.SetStart(true);

	if(data != nullptr)
	{
		ov::ByteStream stream(data);

		while(stream.Remained() > 0)
		{
			size_t read_bytes = stream.Read(static_cast<uint8_t *>(packet.GetData()), RelayPacketDataSize);
			packet.SetDataSize(static_cast<uint16_t>(read_bytes));

			if(stream.Remained() == 0)
			{
				packet.SetEnd(true);
			}

			socket->Send(&packet, sizeof(packet));

			packet.SetStart(false);
		}
	}
	else
	{
		packet.SetEnd(true);

		socket->Send(&packet, sizeof(packet));
	}
}

void RelayServer::Send(const std::shared_ptr<MediaRouteStream> &media_stream, MediaPacket *packet)
{
	if(_client_map.size() == 0)
	{
		return;
	}

	auto stream_info = media_stream->GetStreamInfo();
	auto media_track = stream_info->GetTrack(packet->GetTrackId());

	RelayPacket relay_packet(RelayPacketType::Packet);

	relay_packet.SetMediaType(static_cast<int8_t>(packet->GetMediaType()));
	relay_packet.SetTrackId(static_cast<uint32_t>(packet->GetTrackId()));
	relay_packet.SetPts(static_cast<uint64_t>(packet->GetPts()));
	relay_packet.SetFlags(static_cast<uint8_t>(packet->GetFlags()));

	Send(stream_info->GetId(), relay_packet, packet->GetData().get());
}
