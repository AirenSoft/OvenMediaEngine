//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "profiles/stream_profiles.h"

namespace cfg
{
	struct Stream : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)
		CFG_DECLARE_REF_GETTER_OF(GetProfileList, _profiles.GetProfileList())

	protected:
		void MakeParseList() override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Profiles", &_profiles);
		}

		ov::String _name;
		StreamProfiles _profiles;
	};
}  // namespace cfg