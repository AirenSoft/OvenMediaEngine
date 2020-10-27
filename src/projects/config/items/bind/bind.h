//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./managers/managers.h"
#include "./providers/providers.h"
#include "./publishers/publishers.h"

namespace cfg
{
	namespace bind
	{
		struct Bind : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetManagers, _managers)
			CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
			CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)

		protected:
			void MakeParseList() override
			{
				RegisterValue<Optional>("Managers", &_managers);
				RegisterValue<Optional>("Providers", &_providers);
				RegisterValue<Optional>("Publishers", &_publishers);
			}

			mgr::Managers _managers;
			pvd::Providers _providers;
			pub::Publishers _publishers;
		};
	}  // namespace bind
}  // namespace cfg