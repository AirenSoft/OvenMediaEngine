//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "signed_policy_enabled_modules.h"

namespace cfg
{
	struct Enables : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
		CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		SignedPolicyEnabledModules _providers;
		SignedPolicyEnabledModules _publishers;
	};
}  // namespace cfg