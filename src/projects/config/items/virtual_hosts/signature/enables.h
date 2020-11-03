//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "enables/providers.h"
#include "enables/publishers.h"

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

		SignedPolicyEnabledProviders _providers;
		SignedPolicyEnabledProviders _publishers;
	};
}  // namespace cfg