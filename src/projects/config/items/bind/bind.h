//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "providers/providers.h"
#include "publishers/publishers.h"

namespace cfg
{
	struct Bind : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
		CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Providers", &_providers);
			RegisterValue("Publishers", &_publishers);
		}

		BindProviders _providers;
		BindPublishers _publishers;
	};
}  // namespace cfg