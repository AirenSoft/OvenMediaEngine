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

void RelayClient::Start(const ov::String &application)
{
	OV_ASSERT2(_stop == true);
	_stop = false;

	_media_route_application->RegisterConnectorApp(this->GetSharedPtr());

	_connection = std::thread(
		[&, application]() -> void
		{
			auto data = std::make_shared<ov::Data>(ov::MaxSrtPacketSize);

			auto &origin = _application_info->GetOrigin();

			auto primary = origin.GetPrimary();
			auto secondary = origin.GetSecondary();

			// Extract scheme
			primary = ParseAddress(primary);
			secondary = ParseAddress(secondary);

			if(primary.IsEmpty() && secondary.IsEmpty())
			{
				logte("Could not initialize relay client: both of addresses are invalid");
				return;
			}

			ov::SocketAddress primary_address(primary);
			ov::SocketAddress secondary_address(secondary);

			bool is_primary = true;

			while(_stop == false)
			{
				if((_client_socket.GetState() == ov::SocketState::Closed) && (_client_socket.Create(ov::SocketType::Srt) == false))
				{
					logte("Could not create relay client socket");
					return;
				}

				// Switch between primary and secondary server
				if(
					(is_primary && (primary.IsEmpty())) ||
					((is_primary == false) && (secondary.IsEmpty()))
					)
				{
					is_primary = !is_primary;
				}

				const ov::SocketAddress &address = is_primary ? primary_address : secondary_address;

				logti("Trying to connect to %s origin server...: %s", (is_primary) ? "primary" : "secondary", address.ToString().CStr());

				auto error = _client_socket.Connect(address, 1000);

				if(error != nullptr)
				{
					// retry
					logtw("Cannot connect to %s origin server: %s (%s)", (is_primary) ? "primary" : "secondary", address.ToString().CStr(), error->ToString().CStr());

					// switch
					is_primary = !is_primary;
					continue;
				}

				{
					std::lock_guard<std::mutex> lock_guard(_stream_list_mutex);

					for(auto &stream:_stream_list)
					{
						MediaRouteApplicationConnector::DeleteStream(stream.second->stream_info);
					}

					_stream_list.clear();
				}

				logti("Connected to %s origin server %s successfully", (is_primary) ? "primary" : "secondary", address.ToString().CStr());

				logti("Trying to request register for application [%s]...", application.CStr());

				Register(application);

				while(true)
				{
					// read from server
					error = _client_socket.Recv(data);

					if(error != nullptr)
					{
						logte("An error occurred while receive data: %s", error->ToString().CStr());

						// reconnect
						_client_socket.Close();
						is_primary = !is_primary;
						break;
					}

					RelayPacket packet(data.get());

					switch(packet.GetType())
					{
						case RelayPacketType::Register:
							OV_ASSERT2("RelayClient cannot handle Register packet");
							break;

						case RelayPacketType::CreateStream:
							HandleCreateStream(packet);
							break;

						case RelayPacketType::DeleteStream:
							HandleDeleteStream(packet);
							break;

						case RelayPacketType::Packet:
							// 데이터 처리
							HandleData(packet);
							break;

						case RelayPacketType::Error:
							// There was a problem

							// reconnect
							is_primary = !is_primary;
							_client_socket.Close();
							break;
					}
				}
			}
		});
}

std::shared_ptr<RelayClient::RelayStreamInfo> RelayClient::GetStreamInfo(info::stream_id_t stream_id, bool create_info, bool *created, bool delete_info)
{
	std::lock_guard<std::mutex> lock_guard(_stream_list_mutex);
	std::shared_ptr<RelayStreamInfo> relay_info;
	auto relay_stream = _stream_list.find(stream_id);

	if(relay_stream == _stream_list.end())
	{
		if(create_info)
		{
			// Create a new relay info
			relay_info = std::make_shared<RelayStreamInfo>();
			relay_info->stream_info = std::make_shared<StreamInfo>();

			_stream_list[stream_id] = relay_info;

			if(created != nullptr)
			{
				*created = true;
			}
		}
	}
	else
	{
		relay_info = relay_stream->second;

		if(created != nullptr)
		{
			*created = false;
		}

		if(delete_info)
		{
			_stream_list.erase(stream_id);
		}
	}

	return relay_info;
}

ov::String RelayClient::ParseAddress(const ov::String &address)
{
	if(address.IsEmpty())
	{
		return "";
	}

	auto tokens = address.Split("://");

	if(tokens.size() != 2)
	{
		logte("Invalid relay address: %s", address.CStr());
		return "";
	}

	auto scheme = tokens[0];
	scheme.MakeUpper();

	if(scheme != "SRT")
	{
		logte("Not supported scheme in relay address: %s", address.CStr());
		return "";
	}

	auto new_address = tokens[1];

	if(new_address.IndexOf(':') == -1L)
	{
		new_address.AppendFormat(":%d", RELAY_DEFAULT_PORT);
	}

	return new_address;
}

void RelayClient::HandleCreateStream(const RelayPacket &packet)
{
	ov::String deserialize = ov::String(static_cast<const char *>(packet.GetData()), packet.GetDataSize());

	if(deserialize.IsEmpty())
	{
		// There is no stream to process

		// RelayServer should not send empty CreateStream event
		OV_ASSERT2(false);

		return;
	}

	auto lines = deserialize.Split("\n");

	logtd("Received stream information:\n%s", deserialize.CStr());

	if(lines.empty())
	{
		logte("Invalid packet data: %s", deserialize.CStr());
		return;
	}

	bool first = true;
	std::shared_ptr<StreamInfo> stream_info;
	std::shared_ptr<RelayStreamInfo> relay_info;

	for(auto &line : lines)
	{
		if(first)
		{
			// stream name
			first = false;

			info::stream_id_t stream_id = packet.GetStreamId();

			bool is_created;

			relay_info = GetStreamInfo(stream_id, true, &is_created);

			if(is_created == false)
			{
				logtd("The stream #%u is already created. Ignoring...", stream_id);
				break;
			}

			stream_info = relay_info->stream_info;

			stream_info->SetName(line);
			stream_info->SetId(stream_id);
		}
		else
		{
			if(line.IsEmpty())
			{
				continue;
			}

			if(stream_info == nullptr)
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
			track->SetId(ov::Converter::ToUInt32(info[index++]));
			track->SetCodecId(static_cast<common::MediaCodecId>(ov::Converter::ToInt32(info[index++])));
			track->SetMediaType(static_cast<common::MediaType>(ov::Converter::ToInt32(info[index++])));
			int num = ov::Converter::ToInt32(info[index++]);
			int den = ov::Converter::ToInt32(info[index++]);
			track->SetTimeBase(num, den);
			track->SetBitrate(ov::Converter::ToInt32(info[index++]));
			track->SetStartFrameTime(ov::Converter::ToInt64(info[index++]));
			track->SetLastFrameTime(ov::Converter::ToInt64(info[index++]));

			stream_info->AddTrack(track);

			auto transaction = std::make_shared<Transaction>();

			relay_info->transactions[track->GetId()] = transaction;
		}
	}

	if(stream_info != nullptr)
	{
		MediaRouteApplicationConnector::CreateStream(stream_info);

		logtd("A stream is created: %s/%s (%u/%u)",
		      _application_info->GetName().CStr(), stream_info->GetName().CStr(),
		      packet.GetApplicationId(), stream_info->GetId()
		);
	}
}

void RelayClient::HandleDeleteStream(const RelayPacket &packet)
{
	info::stream_id_t stream_id = packet.GetStreamId();

	auto relay_info = GetStreamInfo(stream_id, false, nullptr, true);

	if(relay_info == nullptr)
	{
		// If a DeleteStream event occurs before registering, it enters here
		return;
	}

	auto stream = relay_info->stream_info;

	logtd("A stream is deleted: %s/%s (%u/%u)",
	      _application_info->GetName().CStr(), stream->GetName().CStr(),
	      packet.GetApplicationId(), stream->GetId()
	);

	MediaRouteApplicationConnector::DeleteStream(stream);
}

void RelayClient::HandleData(const RelayPacket &packet)
{
	auto relay_info = GetStreamInfo(packet.GetStreamId());

	if(relay_info == nullptr)
	{
		// No sync occurred, or sync is in progress
		Register(_application_info->GetName());
		return;
	}

	uint32_t track_id = packet.GetTrackId();
	auto transaction_info = relay_info->transactions.find(track_id);

	if(transaction_info == relay_info->transactions.end())
	{
		// The stream is parsing in HandleCreateStream()
		return;
	}

	std::shared_ptr<Transaction> transaction = transaction_info->second;

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

				::memcpy(media_packet->_frag_hdr.get(), packet.GetFragmentHeader(), sizeof(FragmentationHeader));

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

void RelayClient::Register(const ov::String &identifier)
{
	RelayPacket packet(RelayPacketType::Register);

	packet.SetData(identifier.CStr(), static_cast<uint16_t>(identifier.GetLength()));

	SendPacket(packet);
}

void RelayClient::SendPacket(const RelayPacket &packet)
{
	// TODO: make transaction id
	if(packet.GetTransactionId() == 0)
	{
		RelayPacket new_packet = packet;

		new_packet.SetTransactionId(ov::Random::GenerateUInt32());

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
