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
		bool IsActive() const
		{
			return _active;
		}

		ov::String GetName() const
		{
			return _name;
		}

		const std::vector<AudioProfile> &GetAudioProfiles() const
		{
			return _audio;
		}

		const std::vector<VideoProfile> &GetVideoProfiles() const
		{
			return _video;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Active", &_active);
			RegisterValue("Name", &_name);
			RegisterValue<Optional>("Audio", &_audio);
			RegisterValue<Optional>("Video", &_video);
		}

		bool _active = true;
		ov::String _name;
		std::vector<AudioProfile> _audio;
		std::vector<VideoProfile> _video;
	};
}
