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
	namespace cmn
	{
		struct SingularPort : public Port<int>
		{
		public:
			SingularPort() = default;
			explicit SingularPort(const char *port)
				: Port(port)
			{
			}

		protected:
			void MakeParseList() override
			{
				RegisterValue<ValueType::Text>(nullptr, &_port, nullptr, [this]() -> bool {
					_socket_type = ov::SocketType::Unknown;
					_port_value = ov::Converter::ToInt32(_port.Trim());

					if (IsValidPort(_port_value))
					{
						auto tokens = _port.Split("/");

						// Default: TCP
						_socket_type = (tokens.size() != 2) ? ov::SocketType::Tcp : GetSocketType(tokens[1]);

						return _socket_type != ov::SocketType::Unknown;
					}

					return false;
				});
			}
		};
	}  // namespace cmn
}  // namespace cfg