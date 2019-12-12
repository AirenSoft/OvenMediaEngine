//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "audio_profile.h"
#include "video_profile.h"

namespace cfg
{
	struct Encode : public Item
	{
		CFG_DECLARE_GETTER_OF(IsActive, _active)
		CFG_DECLARE_GETTER_OF(GetName, _name)
		CFG_DECLARE_GETTER_OF(GetAudioProfile, IsParsed(&_audio) ? &_audio : nullptr)
		CFG_DECLARE_GETTER_OF(GetVideoProfile, IsParsed(&_video) ? &_video : nullptr)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Active", &_active);
			RegisterValue("Name", &_name);
			RegisterValue<Optional>("Audio", &_audio);
			RegisterValue<Optional>("Video", &_video);
		}

		bool _active = true;
		ov::String _name;
		AudioProfile _audio;
		VideoProfile _video;
	};
}  // namespace cfg