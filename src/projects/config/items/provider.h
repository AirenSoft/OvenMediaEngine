//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "streams.h"

namespace cfg
{
	enum class ProviderType
	{
		Unknown,
		Rtmp,
	};

	struct Provider : public Item
	{
		Provider(int listen_port)
			: _listen_port(listen_port)
		{
		}

		virtual ProviderType GetType() const = 0;

		int GetListenPort() const
		{
			return _listen_port;
		}

		int GetMaxConnection() const
		{
			return _max_connection;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("IP", &_ip);
			RegisterValue<Optional>("ListenPort", &_listen_port);
			RegisterValue<Optional>("MaxConnection", &_max_connection);
		}

		ov::String _ip;
		int _listen_port = 0;
		int _max_connection = 0;
	};
}