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
	VHostAppName::VHostAppName()
	{
		Append("?InvalidApp?");
	}

	VHostAppName::VHostAppName(const ov::String &vhost_name, const ov::String &app_name)
		: _is_valid(true)
	{
		Format("#%s#%s", vhost_name.Replace("#", "_").CStr(), app_name.Replace("#", "_").CStr());
	}

	bool VHostAppName::Parse(ov::String *vhost_name, ov::String *app_name) const
	{
		if(IsInvalid())
		{
			return false;
		}

		auto tokens = Split("#");

		if (tokens.size() == 3)
		{
			// #<vhost_name>#<app_name>
			OV_ASSERT2(tokens[0] == "");

			if (vhost_name != nullptr)
			{
				*vhost_name = tokens[1];
			}

			if (app_name != nullptr)
			{
				*app_name = tokens[2];
			}
			return true;
		}

		OV_ASSERT2(false);
		return false;
	}

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
		static Application application(Host(cfg::VirtualHost()), InvalidApplicationId, VHostAppName::InvalidVHostAppName(), false);

		return application;
	}
}  // namespace info