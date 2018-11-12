//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream_profile.h"

namespace cfg
{
	struct StreamProfiles : public Item
	{
		const std::vector<StreamProfile> &GetProfiles() const
		{
			return _stream_profile_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Profile", &_stream_profile_list);
		}

		bool PostProcess(int indent)
		{
			return true;
		}

		std::vector<StreamProfile> _stream_profile_list;
	};
}