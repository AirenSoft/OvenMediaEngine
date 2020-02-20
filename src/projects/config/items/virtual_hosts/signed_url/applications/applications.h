//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "application/application.h"

namespace cfg
{
	struct SignedUrlApplications : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetApplicationList, _application_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Application", &_application_list);
		}

		std::vector<SignedUrlApplication> _application_list;
	};
}  // namespace cfg