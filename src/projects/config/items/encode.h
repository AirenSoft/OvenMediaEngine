//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "audio_profile.h"
#include "video_profile.h"

namespace cfg
{
	struct Encode : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Active", &_active);
			result = result && RegisterValue("Name", &_name);
			result = result && RegisterValue("StreamName", &_stream_name);
			result = result && RegisterValue<Optional>("Audio", &_audio);
			result = result && RegisterValue<Optional>("Video", &_video);

			return result;
		}

	protected:
		Value<bool> _active;
		Value<ov::String> _name;
		Value<ov::String> _stream_name;
		Value<AudioProfile> _audio;
		Value<VideoProfile> _video;
	};
}