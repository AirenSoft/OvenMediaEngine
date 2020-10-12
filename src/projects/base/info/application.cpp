//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"
#include "application_private.h"
#include "host.h"

namespace info
{
	Application::Application(const info::Host &host_info, application_id_t app_id, const VHostAppName &name, cfg::Application app_config, bool is_dynamic_app)
		: _application_id(app_id),
		  _name(name),
		  _app_config(std::move(app_config)),
		  _is_dynamic_app(is_dynamic_app)
	{
		_host_info = std::make_shared<info::Host>(host_info);
	}

	Application::Application(const info::Host &host_info, application_id_t app_id, const VHostAppName &name, bool is_dynamic_app)
		: _application_id(app_id),
		  _name(name),
		  _is_dynamic_app(is_dynamic_app)

	{
		_host_info = std::make_shared<info::Host>(host_info);
	}

	const Application &Application::GetInvalidApplication()
	{
		static Application application(Host(cfg::VirtualHost()), InvalidApplicationId, VHostAppName::InvalidAppName(), false);
		return application;
	}
}  // namespace info