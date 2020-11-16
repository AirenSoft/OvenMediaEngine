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
	namespace vhost
	{
		namespace app
		{
			namespace dec
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
			}  // namespace dec
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg