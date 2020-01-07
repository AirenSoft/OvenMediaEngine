//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

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
		CFG_DECLARE_GETTER_OF(GetUse, _use_value)
		CFG_DECLARE_GETTER_OF(IsAudioOnly, GetUse() == StreamProfileUse::AudioOnly)
		CFG_DECLARE_GETTER_OF(IsVideoOnly, GetUse() == StreamProfileUse::VideoOnly)
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Attribute, Optional>("use", &_use, nullptr, [this]() -> bool {
				if (_use == "both")
				{
					_use_value = StreamProfileUse::Both;
				}
				else if (_use == "audio-only")
				{
					_use_value = StreamProfileUse::AudioOnly;
				}
				else if (_use == "video-only")
				{
					_use_value = StreamProfileUse::VideoOnly;
				}
				else
				{
					return false;
				}

				return true;
			});
			RegisterValue<ValueType::Text>(nullptr, &_name);
		}

		ov::String _use = "both";
		StreamProfileUse _use_value = StreamProfileUse::Both;
		ov::String _name;
	};
}  // namespace cfg