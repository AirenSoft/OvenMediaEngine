//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovcrypto/ovcrypto.h>
#include <config/config.h>

#include "stream.h"

namespace info
{
	typedef uint32_t application_id_t;

	class Application : public cfg::Application
	{
	public:
		explicit Application(application_id_t app_id, const cfg::Application &application);
		explicit Application(application_id_t app_id, const ov::String &name, const cfg::Application &application);

		application_id_t GetId() const
		{
			return _application_id;
		}

		template <typename Tpublisher>
		const Tpublisher *GetPublisher() const
		{
			Tpublisher temp_publisher;
			const auto &publishers = GetPublishers().GetPublisherList();

			for (auto &publisher_info : publishers)
			{
				if (temp_publisher.GetType() == publisher_info->GetType())
				{
					return dynamic_cast<const Tpublisher *>(publisher_info);
				}
			}

			return nullptr;
		}

		template <typename Tprovider>
		const Tprovider *GetProvider() const
		{
			Tprovider temp_provider;
			const auto &providers = GetProviders().GetProviderList();

			for (auto &provider_info : providers)
			{
				if (temp_provider.GetType() == provider_info->GetType())
				{
					return dynamic_cast<const Tprovider *>(provider_info);
				}
			}

			return nullptr;
		}

	protected:
		application_id_t _application_id = 0;
	};
}  // namespace info