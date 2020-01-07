//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "decode_video.h"

namespace cfg
{
	struct Decode : public Item
	{
	protected:
		void MakeParseList() override
		{
			RegisterValue("Video", &_video);
		}

		DecodeVideo _video;
	};
}  // namespace cfg