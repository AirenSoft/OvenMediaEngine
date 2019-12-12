//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream_profile.h"

namespace cfg
{
	struct StreamProfiles : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetProfileList, _profile_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Profile", &_profile_list);
		}

		std::vector<StreamProfile> _profile_list;
	};
}  // namespace cfg