//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./helpers.h"

namespace api
{
	std::shared_ptr<ocst::VirtualHost> GetVirtualHost(const std::string_view &vhost_name)
	{
		const auto &vhost_list = ocst::Orchestrator::GetInstance()->GetVirtualHostList();

		for (const auto &vhost : vhost_list)
		{
			if (vhost_name == vhost->name.CStr())
			{
				return vhost;
			}
		}

		return nullptr;
	}

	std::shared_ptr<ocst::Application> GetApplication(const std::shared_ptr<ocst::VirtualHost> &vhost, const std::string_view &app_name)
	{
		if (vhost != nullptr)
		{
			for (auto &item : vhost->app_map)
			{
				if (app_name == item.second->app_info.GetName().GetAppName().CStr())
				{
					return item.second;
				}
			}
		}

		return nullptr;
	}

}  // namespace api
