//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./port.h"

namespace cfg
{
	struct TlsPort : public Port
	{
		explicit TlsPort(const char *port)
			: Port(port),
			  _tls_port(nullptr)
		{
		}

		explicit TlsPort(const char *port, const char *tls_port)
			: Port(port),
			  _tls_port(tls_port)
		{
		}

		CFG_DECLARE_VIRTUAL_GETTER_OF(int, GetTlsPort, _tls_port_value)
		CFG_DECLARE_VIRTUAL_GETTER_OF(ov::SocketType, GetTlsSocketType, _tls_socket_type)

	protected:
		void MakeParseList() override
		{
			Port::MakeParseList();

			RegisterValue<Optional>("TLSPort", &_tls_port, nullptr, [this]() -> bool {
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

		ov::String _tls_port;

		int _tls_port_value = 0;
		ov::SocketType _tls_socket_type = ov::SocketType::Unknown;
	};
}  // namespace cfg