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
			return _use;
		}

		ov::String GetName() const
		{
			return _name;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Attribute>("use", &_use_temp);
			RegisterValue<ValueType::Text>("Name", &_name);
		}

		bool PostProcess(int indent)
		{
			//const ov::String use = *_use_temp.GetValue();
			//
			//if(use == "audio-only")
			//{
			//	_use = StreamProfileUse::AudioOnly;
			//	return true;
			//}
			//else if(use == "video-only")
			//{
			//	_use = StreamProfileUse::VideoOnly;
			//	return true;
			//}
			//else if(use == "both")
			//{
			//	_use = StreamProfileUse::Both;
			//	return true;
			//}

			OV_ASSERT2(false);
			return false;
		}

		ov::String _use_temp = "both";
		StreamProfileUse _use;
		ov::String _name;
	};
}