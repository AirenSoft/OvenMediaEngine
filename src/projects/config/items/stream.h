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
		ov::String GetName() const
		{
			return _name;
		}

		const std::vector<StreamProfile> &GetProfiles() const
		{
			return _profiles.GetProfiles();
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Profiles", &_profiles);
		}

		ov::String _name;
		StreamProfiles _profiles;
	};
}