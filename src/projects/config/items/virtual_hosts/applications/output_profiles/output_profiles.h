//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./output_profile.h"
#include "./hwaccels/hwaccels.h"
#include "./decodes/decodes.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct OutputProfiles : public Item
				{
				protected:
					bool _hwaccel = false;
					HWAccels _hwaccels;
					std::vector<OutputProfile> _output_profiles;
					Decodes _decodes;
				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(IsHardwareAcceleration, _hwaccel);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetHWAccels, _hwaccels);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputProfileList, _output_profiles);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDecodes, _decodes);

				protected:
					void MakeList() override
					{
						// Deprecated
						// Changed to <HWAccels> option.
						Register<Optional>("HardwareAcceleration", &_hwaccel, nullptr,
							[=]() -> std::shared_ptr<ConfigError> {
								logw("Config", "The 'HardwareAcceleration' option is deprecated. Please use the 'HWAccels' option.");
								return nullptr;
							}
						);
						Register<Optional>({"HWAccels", "hwaccels"}, &_hwaccels);
						Register<Optional>("OutputProfile", &_output_profiles);
						Register<Optional>({"Decodes", "decodes"}, &_decodes);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
