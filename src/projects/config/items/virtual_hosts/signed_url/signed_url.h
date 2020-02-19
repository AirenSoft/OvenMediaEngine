//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "applications/applications.h"

namespace cfg
{
	struct SignedUrl : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetApplicationList, _applications.GetApplicationList())

	protected:
		void MakeParseList() override
		{
			RegisterValue("Applications", &_applications);
		}

		SignedUrlApplications _applications;
	};
}  // namespace cfg