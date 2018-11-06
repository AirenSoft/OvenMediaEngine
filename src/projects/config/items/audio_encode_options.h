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
	struct AudioEncodeOptions : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Active", &_active);
			result = result && RegisterValue("Codec", &_codec);
			result = result && RegisterValue("Bitrate", &_bitrate);
			result = result && RegisterValue("Samplerate", &_samplerate);
			result = result && RegisterValue("Channel", &_channel);

			return result;
		}

	protected:
		Value<bool> _active = true;
		Value <ov::String> _codec;
		Value <ov::String> _bitrate;
		Value<int> _samplerate = 0;
		Value<int> _channel = 0;
	};
}