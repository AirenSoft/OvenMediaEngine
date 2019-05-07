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

namespace cfg
{
	enum class PublisherType
	{
		Unknown,
		Webrtc,
		Rtmp,
		Hls,
		Dash,
	};

	struct Publisher : public Item
	{
		Publisher() = default;

		virtual PublisherType GetType() const = 0;

		int GetListenPort() const
		{
			return _listen_port;
		}

		int GetMaxConnection() const
		{
			return _max_connection;
		}

	protected:
		Publisher(int default_listen_port)
			: _listen_port(default_listen_port)
		{
		}

		void MakeParseList() const override
		{
			RegisterValue<Optional>("ListenPort", &_listen_port);
			RegisterValue<Optional>("MaxConnection", &_max_connection);
		}

		int _listen_port = 0;
		int _max_connection = 0;
	};
}