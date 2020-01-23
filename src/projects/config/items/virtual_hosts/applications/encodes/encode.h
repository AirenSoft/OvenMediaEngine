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
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)
		CFG_DECLARE_REF_GETTER_OF(GetAudioProfileList, _audio_list)
		CFG_DECLARE_REF_GETTER_OF(GetVideoProfileList, _video_list)

		// Temporary code to use before supporting multi-encode setup
		const AudioProfile *GetAudioProfile() const
		{
			if(_audio_list.empty())
			{
				return nullptr;
			}

			return &(_audio_list.at(0));
		}

		const VideoProfile *GetVideoProfile() const
		{
			if(_video_list.empty())
			{
				return nullptr;
			}

			return &(_video_list.at(0));
		}

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Active", &_active);
			RegisterValue("Name", &_name);
			RegisterValue<Optional>("Audio", &_audio_list);
			RegisterValue<Optional>("Video", &_video_list);
		}

		bool _active = true;
		ov::String _name;
		std::vector<AudioProfile> _audio_list;
		std::vector<VideoProfile> _video_list;
	};
}  // namespace cfg