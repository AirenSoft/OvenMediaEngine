//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "web_socket_client.h"

#include <unistd.h>

#include <algorithm>

#include "./web_socket_private.h"
#include "web_socket_datastructure.h"

namespace http
{
	namespace svr
	{
		namespace ws
		{
			Client::Client(const std::shared_ptr<HttpConnection> &client)
				: _client(client)
			{
			}

			Client::~Client()
			{
			}

			ssize_t Client::Send(const std::shared_ptr<const ov::Data> &data, FrameOpcode opcode)
			{
				// RFC6455 - 5.2.  Base Framing Protocol
				//
				//
				FrameHeader header{
					.opcode = static_cast<uint8_t>(opcode),
					.reserved = 0x00,
					.fin = true,
					.payload_length = 0,
					.mask = false};

				size_t length = (data == nullptr) ? 0LL : data->GetLength();

				if (length < 0x7D)
				{
					// frame-payload-length    = ( %x00-7D )
					//                         / ( %x7E frame-payload-length-16 )
					//                         / ( %x7F frame-payload-length-63 )
					//                         ; 7, 7+16, or 7+64 bits in length,
					//                         ; respectively
					header.payload_length = static_cast<uint8_t>(length);
				}
				else if (length < 0xFFFF)
				{
					// frame-payload-length-16 = %x0000-FFFF ; 16 bits in length
					header.payload_length = 126;
				}
				else
				{
					// frame-payload-length-63 = %x0000000000000000-7FFFFFFFFFFFFFFF
					//                         ; 64 bits in length
					header.payload_length = 127;
				}

				auto response = _client->GetResponse();

				response->Send(&header);

				if (header.payload_length == 126)
				{
					auto payload_length = ov::HostToNetwork16(static_cast<uint16_t>(length));

					response->Send(&payload_length);
				}
				else if (header.payload_length == 127)
				{
					auto payload_length = ov::HostToNetwork64(static_cast<uint64_t>(length));

					response->Send(&payload_length);
				}

				if (length > 0LL)
				{
					logtd("Trying to send data\n%s", data->Dump(32).CStr());

					return response->Send(data) ? length : -1LL;
				}

				return length;
			}

			ssize_t Client::Send(const std::shared_ptr<const ov::Data> &data)
			{
				return Send(data, FrameOpcode::Binary);
			}

			ssize_t Client::Send(const ov::String &string)
			{
				return Send(string.ToData(false), FrameOpcode::Text);
			}

			ssize_t Client::Send(const Json::Value &value)
			{
				return Send(ov::Json::Stringify(value));
			}

			void Client::Close()
			{
				_client->GetResponse()->Close();
			}

			ov::String Client::ToString() const
			{
				return ov::String::FormatString("<WebSocketClient: %p, %s>", this, _client->GetRequest()->GetRemote()->ToString().CStr());
			}

			void Client::AddData(ov::String key, std::variant<bool, uint64_t, ov::String> value)
			{
				_data_map.emplace(key, value);
			}

			std::tuple<bool, std::variant<bool, uint64_t, ov::String>> Client::GetData(ov::String key)
			{
				if(_data_map.find(key) == _data_map.end())
				{
					return {false, false};
				}

				return {true, _data_map[key]};
			}

		}  // namespace ws
	}	   // namespace svr
}  // namespace http
