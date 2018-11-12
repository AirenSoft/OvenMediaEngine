//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "application.h"

namespace cfg
{
	struct Applications : public Item
	{
		const std::vector<Application> &GetApplications() const
		{
			return _application_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional, Includable>("Application", &_application_list);
		}

		std::vector<Application> _application_list;
	};
}