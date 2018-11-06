//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_encode_options.h"

namespace cfg
{
	struct VideoProfile : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Bypass", &_bypass);
			result = result && RegisterValue<Optional>("VideoEncodeOptions", &_audio_encode_options);

			return result;
		}

	protected:
		Value<bool> _bypass = false;
		Value<VideoEncodeOptions> _audio_encode_options;
	};
}