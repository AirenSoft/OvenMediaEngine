//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
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
					std::vector<OutputProfile> _output_profiles;

				public:
					CFG_DECLARE_REF_GETTER_OF(GetOutputProfileList, _output_profiles)

				protected:
					void MakeParseList() override
					{
						RegisterValue<Optional>("OutputProfiles", &_output_profiles);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
