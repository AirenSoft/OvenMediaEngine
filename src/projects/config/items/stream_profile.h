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
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<ValueType::Attribute>("use", &_use_temp);
			result = result && RegisterValue<ValueType::Text>("Name", &_name);

			return result;
		}

		StreamProfileUse GetUse() const
		{
			return _use;
		}

		ov::String GetName() const
		{
			return *_name;
		}

	protected:
		bool PostProcess(int indent) override
		{
			const ov::String use = *_use_temp;

			if(use == "audio-only")
			{
				_use = StreamProfileUse::AudioOnly;
				return true;
			}
			else if(use == "video-only")
			{
				_use = StreamProfileUse::VideoOnly;
				return true;
			}
			else if(use == "both")
			{
				_use = StreamProfileUse::Both;
				return true;
			}

			OV_ASSERT2(false);
			return false;
		}

		Value <ov::String> _use_temp = "both";
		StreamProfileUse _use;
		Value <ov::String> _name;
	};
}