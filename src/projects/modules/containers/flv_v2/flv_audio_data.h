//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./flv_common.h"

namespace modules
{
	namespace flv
	{
		struct AudioData : public CommonData
		{
			AudioData(SoundFormat sound_format)
				: sound_format(sound_format)
			{
			}

			const SoundFormat sound_format;
			SoundRate sound_rate;
			SoundSize sound_size;
			SoundType sound_type;

			// AAC only
			AACPacketType aac_packet_type;
		};
	}  // namespace flv
}  // namespace modules
