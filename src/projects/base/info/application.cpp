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
	Application::Application(application_id_t app_id, const cfg::Application &application)
		: cfg::Application(application),
		  _application_id(app_id)
	{
	}

	Application::Application(application_id_t app_id, const ov::String &name, const cfg::Application &application)
		: Application(app_id, application)
	{
		_name = name;
	}

}  // namespace info