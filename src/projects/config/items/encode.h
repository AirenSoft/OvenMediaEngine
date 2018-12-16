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

		const AudioProfile *GetAudioProfile() const
		{
			return IsParsed(&_audio) ? &_audio : nullptr;
		}

		const VideoProfile *GetVideoProfile() const
		{
			return IsParsed(&_video) ? &_video : nullptr;
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
		AudioProfile _audio;
		VideoProfile _video;
	};
}