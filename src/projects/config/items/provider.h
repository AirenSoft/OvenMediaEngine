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
		virtual ProviderType GetType() const = 0;

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("IP", &_ip);
			RegisterValue<Optional>("MaxConnection", &_max_connection);
		}

		ov::String _ip;
		int _max_connection = 0;
	};
}