//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

#include <base/ovsocket/socket.h>

namespace cfg
{
	struct Port : public Item
	{
		explicit Port(const char *port)
			: _port(port)
		{
		}

		virtual int GetPort() const
		{
			return ov::Converter::ToInt32(_port);
		}

		virtual ov::SocketType GetSocketType() const
		{
			auto tokens = _port.Split("/");

			if(tokens.size() != 2)
			{
				return ov::SocketType::Tcp;
			}

			auto type = tokens[1].UpperCaseString();

			if(type == "TCP")
			{
				return ov::SocketType::Tcp;
			}
			else if(type == "UDP")
			{
				return ov::SocketType::Udp;
			}
			else if(type == "SRT")
			{
				return ov::SocketType::Srt;
			}

			return ov::SocketType::Unknown;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text>(nullptr, &_port);
		}

		ov::String _port;
	};
}