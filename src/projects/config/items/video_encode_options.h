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
	struct VideoEncodeOptions : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Active", &_active);
			result = result && RegisterValue<Optional>("HWAcceleration", &_hw_acceleration);
			result = result && RegisterValue("Codec", &_codec);
			result = result && RegisterValue("Scale", &_scale);
			result = result && RegisterValue("Width", &_width);
			result = result && RegisterValue("Height", &_height);
			result = result && RegisterValue("Bitrate", &_bitrate);
			result = result && RegisterValue("Framerate", &_framerate);

			return result;
		}

	protected:
		Value<bool> _active = true;
		Value <ov::String> _hw_acceleration = "none";
		Value <ov::String> _codec;
		Value <ov::String> _scale;
		Value<int> _width = 0;
		Value<int> _height = 0;
		Value <ov::String> _bitrate;
		Value<float> _framerate = 0.0f;
	};
}