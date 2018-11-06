//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream_profiles.h"

namespace cfg
{
	struct Stream : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("Name", &_name);
			result = result && RegisterValue("Profiles", &_profiles);

			return result;
		}

		//const std::vector<std::shared_ptr<StreamProfile>> &GetProfiles() const
		//{
		//	return _profiles.GetValueAs<StreamProfiles>()->GetProfiles();
		//}

	protected:
		Value<ov::String> _name;
		Value<StreamProfiles> _profiles;
	};
}