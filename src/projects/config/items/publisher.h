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
		virtual PublisherType GetType() const = 0;

		ov::String GetIp() const
		{
			return _ip;
		}

		int GetMaxConnection() const
		{
			return _max_connection;
		}

		const Streams &GetStreams() const
		{
			return _streams;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("IP", &_ip);
			RegisterValue<Optional>("MaxConnection", &_max_connection);
			RegisterValue<Optional>("Streams", &_streams);
		}

		ov::String _ip;
		int _max_connection = 0;
		Streams _streams;
	};
}