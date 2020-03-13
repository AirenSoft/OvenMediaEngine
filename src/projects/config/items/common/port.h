//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovsocket/socket.h>
#include <base/ovlibrary/converter.h>

namespace cfg
{
	struct Port : public Item
	{
		explicit Port(const char *port)
			: _port(port),
			  _tls_port(nullptr)
		{
		}

		explicit Port(const char *port, const char *tls_port)
			: _port(port),
			  _tls_port(tls_port)
		{
		}

		CFG_DECLARE_VIRTUAL_GETTER_OF(int, GetPort, _port_value)
		CFG_DECLARE_VIRTUAL_GETTER_OF(ov::SocketType, GetSocketType, _socket_type)

		CFG_DECLARE_VIRTUAL_GETTER_OF(int, GetTlsPort, _tls_port_value)
		CFG_DECLARE_VIRTUAL_GETTER_OF(ov::SocketType, GetTlsSocketType, _tls_socket_type)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Port", &_port, nullptr, [this]() -> bool {
				_socket_type = ov::SocketType::Unknown;

				_port_value = ov::Converter::ToInt32(_port);

				if ((_port_value <= 0) || (_port_value >= 65536))
				{
					return false;
				}

				auto tokens = _port.Split("/");

				if (tokens.size() != 2)
				{
					// Default: TCP
					_socket_type = ov::SocketType::Tcp;
				}
				else
				{
					auto type = tokens[1].UpperCaseString();

					if (type == "TCP")
					{
						_socket_type = ov::SocketType::Tcp;
					}
					else if (type == "UDP")
					{
						_socket_type = ov::SocketType::Udp;
					}
					else if (type == "SRT")
					{
						_socket_type = ov::SocketType::Srt;
					}
				}

				return _socket_type != ov::SocketType::Unknown;
			});

			RegisterValue<Optional>("TlsPort", &_tls_port, nullptr, [this]() -> bool {
				_tls_socket_type = ov::SocketType::Unknown;

				_tls_port_value = ov::Converter::ToInt32(_tls_port);

				if ((_tls_port_value <= 0) || (_tls_port_value >= 65536))
				{
					return false;
				}

				auto tokens = _port.Split("/");

				if (tokens.size() != 2)
				{
					// Default: TCP
					_tls_socket_type = ov::SocketType::Tcp;
				}
				else
				{
					auto type = tokens[1].UpperCaseString();

					if (type == "TCP")
					{
						_tls_socket_type = ov::SocketType::Tcp;
					}
					else if (type == "UDP")
					{
						_tls_socket_type = ov::SocketType::Udp;
					}
					else if (type == "SRT")
					{
						_tls_socket_type = ov::SocketType::Srt;
					}
				}

				return _tls_socket_type != ov::SocketType::Unknown;
			});
		}

		ov::String _port;
		ov::String _tls_port;

		int _port_value = 0;
		ov::SocketType _socket_type = ov::SocketType::Unknown;

		int _tls_port_value = 0;
		ov::SocketType _tls_socket_type = ov::SocketType::Unknown;
	};
}  // namespace cfg