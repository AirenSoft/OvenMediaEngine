//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	enum class StreamProfileUse
	{
		// audio-only
			AudioOnly,
		// video-only
			VideoOnly,
		// both
			Both
	};

	struct StreamProfile : public Item
	{
		StreamProfileUse GetUse() const
		{
			if(_use == "audio-only")
			{
				return StreamProfileUse::AudioOnly;
			}
			else if(_use == "video-only")
			{
				return StreamProfileUse::VideoOnly;
			}

			return StreamProfileUse::Both;
		}

		bool IsAudioOnly() const
		{
			return GetUse() == StreamProfileUse::AudioOnly;
		}

		bool IsVideoOnly() const
		{
			return GetUse() == StreamProfileUse::VideoOnly;
		}

		ov::String GetName() const
		{
			return _name;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Attribute, Optional>("use", &_use);
			RegisterValue<ValueType::Text>(nullptr, &_name);
		}

		ov::String _use = "both";
		ov::String _name;
	};
}