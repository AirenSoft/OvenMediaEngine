//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhost_controller.h"

#include <config/config.h>

#include "../../../api_private.h"
#include "apps/apps_controller.h"

namespace api
{
	namespace v1
	{
		void VHostController::PrepareHandlers()
		{
			AppendPrefix("/vhosts");

			CreateSubController<v1::AppsController>();

			GetHandler("", &VHostController::OnGetVhostList);
			GetHandler("/(?<vhost>[^/]+)", &VHostController::OnGetVhost);
		}

		HttpNextHandler VHostController::OnGetVhostList(const std::shared_ptr<HttpClient> &client)
		{
			auto vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();
			Json::Value response(Json::ValueType::arrayValue);

			for (const auto &vhost : vhost_list_config)
			{
				response.append(vhost.GetName().CStr());
			}

#if DEBUG
			auto response_string = ov::Json::Stringify(response, true);
			response_string.Append('\n');
#else	// DEBUG
			auto response_string = ov::Json::Stringify(response);
#endif	// DEBUG

			client->GetResponse()->AppendString(response_string);

			return HttpNextHandler::DoNotCall;
		}

		HttpNextHandler VHostController::OnGetVhost(const std::shared_ptr<HttpClient> &client)
		{
			auto vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();
			auto &uri = client->GetRequest()->GetRequestTarget();
			auto tokens = uri.Split("/");

			// tokens[0] = ""
			// tokens[1] = "v1"
			// tokens[2] = "vhosts"
			// tokens[3] = "<vhost_name>"
			auto vhost_name = tokens[3];
			Json::Value response(Json::ValueType::objectValue);

			logti("Get the virtual host information for %s", vhost_name.CStr());

			bool found = false;

			for (const auto &vhost : vhost_list_config)
			{
				if (vhost_name == vhost.GetName())
				{
					found = true;
					break;
				}
			}

			if (found)
			{
				// TODO(dimiden): Fill this information
				response["name"] = vhost_name.CStr();
			}
			else
			{
				auto message = ov::String::FormatString("Could not find virtual host: %s", vhost_name.CStr());

				client->GetResponse()->SetStatusCode(HttpStatusCode::NotFound);
				response["message"] = message.CStr();
			}

#if DEBUG
			auto response_string = ov::Json::Stringify(response, true);
			response_string.Append('\n');
#else	// DEBUG
			auto response_string = ov::Json::Stringify(response);
#endif	// DEBUG

			client->GetResponse()->AppendString(response_string);

			return HttpNextHandler::DoNotCall;
		}

	}  // namespace v1
}  // namespace api
