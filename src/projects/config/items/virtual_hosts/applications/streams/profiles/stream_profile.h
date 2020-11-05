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
	namespace vhost
	{
		namespace app
		{
			namespace stream
			{
				namespace prf
				{
					enum class Use
					{
						// audio-only
						AudioOnly,
						// video-only
						VideoOnly,
						// both
						Both
					};

					struct Profile : public Item
					{
						CFG_DECLARE_GETTER_OF(GetUse, _use_value)
						CFG_DECLARE_GETTER_OF(IsAudioOnly, GetUse() == Use::AudioOnly)
						CFG_DECLARE_GETTER_OF(IsVideoOnly, GetUse() == Use::VideoOnly)
						CFG_DECLARE_REF_GETTER_OF(GetName, _name)

					protected:
						void MakeParseList() override
						{
							RegisterValue<ValueType::Attribute, Optional>("use", &_use, nullptr, [this]() -> bool {
								if (_use == "both")
								{
									_use_value = Use::Both;
								}
								else if (_use == "audio-only")
								{
									_use_value = Use::AudioOnly;
								}
								else if (_use == "video-only")
								{
									_use_value = Use::VideoOnly;
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
						Use _use_value = Use::Both;
						ov::String _name;
					};
				}  // namespace prf
			}	   // namespace stream
		}		   // namespace app
	}			   // namespace vhost
}  // namespace cfg