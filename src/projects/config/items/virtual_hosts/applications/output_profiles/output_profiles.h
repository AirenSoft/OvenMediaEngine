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
					std::vector<OutputProfile> _output_profiles;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(IsHardwareAcceleration, _hwaccel);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputProfileList, _output_profiles);

				protected:
					void MakeList() override
					{
						Register<Optional>("HardwareAcceleration", &_hwaccel);
						Register<Optional>("OutputProfile", &_output_profiles);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
