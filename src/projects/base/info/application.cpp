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

namespace info
{
	Application::Application(application_id_t app_id, cfg::Application app_config)
		: _application_id(app_id),
		  _app_config(std::move(app_config)),
		  _name(app_config.GetName())
	{
	}

	Application::Application(application_id_t app_id, const ov::String &name)
		: _application_id(app_id),
		  _name(name)
	{
	}

	const Application &Application::GetInvalidApplication()
	{
		static Application application;
		
		return application;
	}
}  // namespace info