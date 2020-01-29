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
	Application::Application(const info::Host &host_info, application_id_t app_id, const ov::String &name, cfg::Application app_config)
		:	_application_id(app_id),
		  	_name(name),
		  	_app_config(std::move(app_config))
	{
		_host_info = std::make_shared<info::Host>(host_info);
	}

	Application::Application(const info::Host &host_info, application_id_t app_id, const ov::String &name)
		:	_application_id(app_id),
		  	_name(name)
	{
		_host_info = std::make_shared<info::Host>(host_info);
	}

	const Application &Application::GetInvalidApplication()
	{
		static Application application(Host(cfg::VirtualHost()), InvalidApplicationId, "?InvalidApp?");
		return application;
	}
}  // namespace info