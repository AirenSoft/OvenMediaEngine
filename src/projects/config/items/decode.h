//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "decode_video.h"

namespace cfg
{
	struct Decode : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("Video", &_video);

			return result;
		}

	protected:
		Value<DecodeVideo> _video;
	};
}