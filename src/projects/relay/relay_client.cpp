//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "relay_client.h"
#include "relay_private.h"

#include <base/provider/stream.h>

void RelayClient::Start()
{
	OV_ASSERT2(_stop == true);
	_stop = false;

	_media_route_application->RegisterConnectorApp(this->GetSharedPtr());

	_connection = std::thread(
		[&]() -> void
		{
			auto data = std::make_shared<ov::Data>(ov::MaxSrtPacketSize);
			ov::SocketAddress address(_server_address);

			while(_stop == false)
			{
				if((_client_socket.GetState() == ov::SocketState::Closed) && (_client_socket.Create(ov::SocketType::Srt) == false))
				{
					return;
				}

				logti("Trying to connect to origin server...: %s", address.ToString().CStr());

				if(_client_socket.Connect(address, 1000) == false)
				{
					// retry
					sleep(1);

					continue;
				}

				logti("Connected to %s", address.ToString().CStr());

				while(true)
				{
					// read from server
					auto error = _client_socket.Recv(data);

					if(error != nullptr)
					{
						logte("An error occurred while receive data: %s", error->ToString().CStr());
						_client_socket.Close();

						// reconnect
						break;
					}

					RelayPacket packet(data.get());

					switch(packet.GetType())
					{
						case RelayPacketType::Sync:
							HandleSync(packet);
							break;

						case RelayPacketType::Packet:
							// 데이터 처리
							HandleData(packet);
							break;

						default:
							logtw("Unknown packet type: %d", packet.GetType());
							break;
					}
				}
			}
		});
}

void RelayClient::HandleSync(const RelayPacket &packet)
{
	ov::String deserialize = ov::String(static_cast<const char *>(packet.GetData()), packet.GetDataSize());

	if(deserialize.IsEmpty())
	{
		// Stream is deleted from origin
		auto stream_list = _media_route_application->GetStreams();
		auto stream = stream_list[packet.GetStreamId()];

		if(stream != nullptr)
		{
			logti("Stream is deleted (%d, %s)", packet.GetStreamId(), stream->GetStreamInfo()->GetName().CStr());
			_media_route_application->OnDeleteStream(this->GetSharedPtr(), stream->GetStreamInfo());
		}
		else
		{
			logti("Stream is deleted (unknown stream: %d)", packet.GetStreamId());
		}

		return;
	}

	auto lines = deserialize.Split("\n");
	logtd("Received sync data:\n%s", deserialize.CStr());

	if(lines.empty())
	{
		logte("Invalid packet data: %s", deserialize.CStr());
		return;
	}

	bool first = true;
	auto stream = std::make_shared<StreamInfo>();

	for(auto &line : lines)
	{
		if(first)
		{
			// stream name
			first = false;

			stream->SetName(line);
			stream->SetId(packet.GetStreamId());

			{
				std::lock_guard<std::mutex> lock_guard(_transaction_lock);
				_transactions[stream->GetId()].clear();
			}
		}
		else
		{
			if(line.GetLength() == 0)
			{
				continue;
			}

			// track info
			auto info = line.Split("|");

			if(info.size() != 14)
			{
				logte("Invalid track data: [%s]", line.CStr());
				return;
			}

			auto track = std::make_shared<MediaTrack>();
			int index = 0;

			// Deserialize VideoTrack
			// framerate|width|height
			track->SetFrameRate(ov::Converter::ToDouble(info[index++]));
			track->SetWidth(ov::Converter::ToInt32(info[index++]));
			track->SetHeight(ov::Converter::ToInt32(info[index++]));

			// Deserialize AudioTrack
			// samplerate|format|layout
			track->SetSampleRate(ov::Converter::ToInt32(info[index++]));
			track->GetSample().SetFormat(static_cast<common::AudioSample::Format>(ov::Converter::ToInt32(info[index++])));
			track->GetChannel().SetLayout(static_cast<common::AudioChannel::Layout >(ov::Converter::ToInt32(info[index++])));

			// Serialize MediaTrack
			// track_id|codec_id|media_type|timebase.num|timebase.den|bitrate|start_frame_time|last_frame_time
			track->SetId(ov::Converter::ToInt32(info[index++]));
			track->SetCodecId(static_cast<common::MediaCodecId>(ov::Converter::ToInt32(info[index++])));
			track->SetMediaType(static_cast<common::MediaType>(ov::Converter::ToInt32(info[index++])));
			int num = ov::Converter::ToInt32(info[index++]);
			int den = ov::Converter::ToInt32(info[index++]);
			track->SetTimeBase(num, den);
			track->SetBitrate(ov::Converter::ToInt32(info[index++]));
			track->SetStartFrameTime(ov::Converter::ToInt64(info[index++]));
			track->SetLastFrameTime(ov::Converter::ToInt64(info[index++]));

			// TODO: change to iterator method to prevent DoS attack
			{
				std::lock_guard<std::mutex> lock_guard(_transaction_lock);
				_transactions[stream->GetId()][track->GetId()] = std::make_shared<Transaction>();
			}

			stream->AddTrack(track);
		}
	}

	MediaRouteApplicationConnector::CreateStream(stream);

	logtd("Sync %u/%u", packet.GetApplicationId(), stream->GetId());
}

void RelayClient::HandleData(const RelayPacket &packet)
{
	std::shared_ptr<Transaction> transaction;

	{
		std::lock_guard<std::mutex> lock_guard(_transaction_lock);
		// TODO: change to iterator method to prevent DoS attack
		transaction = _transactions[packet.GetStreamId()][packet.GetTrackId()];
	}

	if(transaction == nullptr)
	{
		logtd("There is no transaction for stream: %u", packet.GetStreamId());
		return;
	}

	auto &data = transaction->data;
	bool is_different_packet = false;

	if(packet.GetTransactionId() != transaction->transaction_id)
	{
		// another packet is arrived
		is_different_packet = true;
	}
	else
	{
		data.Append(packet.GetData(), packet.GetDataSize());
	}

	if(packet.IsEnd() || is_different_packet)
	{
		// send to media router
		auto stream_list = _media_route_application->GetStreams();
		auto stream = stream_list[packet.GetStreamId()];

		if(stream != nullptr)
		{
			if(data.GetLength() > 0)
			{
				auto media_packet = std::make_unique<MediaPacket>(
					static_cast<common::MediaType>(transaction->media_type),
					transaction->track_id,
					data.GetData(),
					data.GetLength(),
					transaction->last_pts,
					static_cast<MediaPacketFlag>(transaction->flags)
				);

				_media_route_application->OnReceiveBuffer(this->GetSharedPtr(), stream->GetStreamInfo(), std::move(media_packet));
			}
		}
		else
		{
			OV_ASSERT2(stream != nullptr);
		}

		// clear the data
		data.Clear();
	}

	if(is_different_packet)
	{
		transaction->transaction_id = packet.GetTransactionId();
		transaction->media_type = packet.GetMediaType();
		transaction->last_pts = packet.GetPts();
		transaction->track_id = packet.GetTrackId();
		data.Append(packet.GetData(), packet.GetDataSize());
		transaction->flags = packet.GetFlags();
	}
}

void RelayClient::Sync(const ov::String &identifier)
{
	RelayPacket packet(RelayPacketType::RequestSync);

	packet.SetData(identifier.CStr(), static_cast<uint16_t>(identifier.GetLength()));

	SendPacket(packet);
}

void RelayClient::SendPacket(const RelayPacket &packet)
{
	// TODO: make transaction id
	if(packet.GetTransactionId() == 0)
	{
		RelayPacket new_packet = packet;

		new_packet.SetTransactionId(ov::Random::GenerateInteger());

		_client_socket.Send(&new_packet, sizeof(new_packet));
	}
	else
	{
		_client_socket.Send(&packet, sizeof(packet));
	}
}

void RelayClient::SendPacket(RelayPacketType type, info::application_id_t application_id, info::stream_id_t stream_id, const MediaPacket &packet)
{
	RelayPacket relay_packet(type);

	relay_packet.SetApplicationId(ov::HostToBE32(application_id));
	relay_packet.SetStreamId(ov::HostToBE32(stream_id));

	SendPacket(relay_packet);
}

void RelayClient::Stop()
{
	_stop = false;

	if(_connection.joinable())
	{
		_connection.join();
	}
}
