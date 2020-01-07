//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "orchestrator.h"
#include "orchestrator_private.h"

#include <base/media_route/media_route_interface.h>

bool Orchestrator::PrepareOriginMap(const cfg::Origins &origins)
{
	std::lock_guard<decltype(_origin_map_mutex)> lock_guard(_origin_map_mutex);

	// Create a mapping table for origins
	const auto &origin_list = origins.GetOriginList();

	_origin_list.clear();

	logtd("Origin map: (%zu items)", origin_list.size());

	for (auto origin : origin_list)
	{
		auto &pass = origin.GetPass();
		logtd("  > %s ", origin.GetLocation().CStr());

		for (auto url : pass.GetUrlList())
		{
			logtd("    - %s", url.GetUrl().CStr());
		}

		_origin_list.emplace_back(std::make_shared<Origin>(origin));
	}
}

bool Orchestrator::RegisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		OV_ASSERT2(module != nullptr);
		return false;
	}

	auto type = module->GetModuleType();

	// Check if module exists
	std::lock_guard<__decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto &info : _modules)
	{
		if (info.module == module)
		{
			if (info.type == type)
			{
				logtw("[%p] %s module is already registered", module.get(), GetOrchestratorModuleTypeName(type));
			}
			else
			{
				logtw("[%p] The module type was %s, but now %s", module.get(), GetOrchestratorModuleTypeName(info.type), GetOrchestratorModuleTypeName(type));
			}

			OV_ASSERT2(false);
			return false;
		}
	}

	_modules.emplace_back(type, module);
	auto &list = _module_map[type];
	list.push_back(module);

	logtd("[%p] %s module is registered", module.get(), GetOrchestratorModuleTypeName(type));

	return true;
}

bool Orchestrator::UnregisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		OV_ASSERT2(module != nullptr);
		return false;
	}

	std::lock_guard<__decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto info = _modules.begin(); info != _modules.end(); ++info)
	{
		if (info->module == module)
		{
			_modules.erase(info);
			auto &list = _module_map[info->type];
			logtd("[%p] %s module is unregistered", module.get(), GetOrchestratorModuleTypeName(info->type));
			return true;
		}
	}

	logtw("[%p] %s module not found", module.get(), GetOrchestratorModuleTypeName(module->GetModuleType()));
	OV_ASSERT2(false);

	return false;
}

info::application_id_t Orchestrator::GenerateApplicationId() const
{
	while (true)
	{
		info::application_id_t app_id = ov::Random::GenerateUInt32();

		if (app_id == 0)
		{
			continue;
		}

		if (_app_map.find(app_id) == _app_map.end())
		{
			return app_id;
		}
	}
}

std::shared_ptr<pvd::Provider> Orchestrator::GetProviderForScheme(const ov::String &scheme)
{
	auto lower_scheme = scheme.LowerCaseString();

	logtd("Obtaining ProviderType for scheme %s...", scheme.CStr());

	ProviderType type;

	if (lower_scheme == "rtmp")
	{
		type = ProviderType::Rtmp;
	}
	else if (lower_scheme == "rtsp")
	{
		type = ProviderType::Rtsp;
	}
	else if (lower_scheme == "ovt")
	{
		type = ProviderType::Ovt;
	}
	else
	{
		logte("Could not find a provider for scheme %s", scheme.CStr());
		return nullptr;
	}

	// Find the provider
	std::shared_ptr<OrchestratorProviderModuleInterface> module = nullptr;
	bool succeeded = false;

	for (auto info = _modules.begin(); info != _modules.end(); ++info)
	{
		if (info->type == OrchestratorModuleType::Provider)
		{
			auto module = std::dynamic_pointer_cast<OrchestratorProviderModuleInterface>(info->module);
			auto provider = std::dynamic_pointer_cast<pvd::Provider>(module);

			if (provider == nullptr)
			{
				logtd("Provider must inherit from pvd::Provider");
				OV_ASSERT2(provider != nullptr);
				continue;
			}

			if (provider->GetProviderType() == type)
			{
				return provider;
			}
		}
	}

	logtw("Provider (%d) is not found for scheme %s", type, scheme.CStr());
	return nullptr;
}

std::shared_ptr<pvd::Provider> Orchestrator::GetProviderForUrl(const ov::String &url)
{
	// Find a provider type using the scheme
	auto parsed_url = ov::Url::Parse(url.CStr());

	if (url == nullptr)
	{
		logtw("Could not parse URL: %s", url.CStr());
		return nullptr;
	}

	logtd("Obtaining ProviderType for URL %s...", url.CStr());

	return GetProviderForScheme(parsed_url->Scheme());
}

std::shared_ptr<Orchestrator::Origin> Orchestrator::GetUrlListForLocation(const ov::String &location, ov::String *app_name, ov::String *stream_name, std::vector<ov::String> *url_list)
{
	// Find the origin using the location
	for (auto &origin : _origin_list)
	{
		logtd("Trying to find the item that match location: %s", location.CStr());

		// TODO(dimien): Replace with the regex
		if (location.HasPrefix(origin->location))
		{
			// If the location has the prefix that configured in <Origins>, extract the remaining part
			// For example, if the settings is:
			//      <Origin>
			//      	<Location>/app/stream</Location>
			//      	<Pass>
			//      		<Url>ovt://origin.airensoft.com:9000/another_app/and_stream</Url>
			//      	</Pass>
			//      </Origin>
			// And when the location is "/app/stream_o",
			//
			// <Location>: /app/stream
			// location:   /app/stream_o
			//                        ~~ <= remaining part
			auto remaining_part = location.Substring(origin->location.GetLength());

			// Obtain the application name and the stream name
			auto tokens = location.Split("/");

			switch (tokens.size())
			{
				default:
				case 3:
					// "/app/stream"
					*app_name = tokens[1];
					*stream_name = tokens[2];
					break;

				case 2:
					// "app/stream"
					*app_name = tokens[0];
					*stream_name = tokens[1];
					break;

				case 1:
				case 0:
					logte("Invalid location: %s", location.CStr());
					return nullptr;
			}

			if (app_name->IsEmpty() || stream_name->IsEmpty())
			{
				logte("Invalid location: %s (%s, %s)", app_name->CStr(), stream_name->CStr());
				return nullptr;
			}

			logtd("Found: location: %s (app: %s, stream: %s), remaining_part: %s", origin->location.CStr(), app_name->CStr(), stream_name->CStr(), remaining_part.CStr());

			for (auto url : origin->url_list)
			{
				// Append the remaining_part to the URL

				// For example,
				//    url:     ovt://origin.airensoft.com:9000/another_app/and_stream
				//    new_url: ovt://origin.airensoft.com:9000/another_app/and_stream_o
				//                                                                   ~~ <= remaining part

				// Prepend "<scheme>://"
				url.Prepend("://");
				url.Prepend(origin->scheme);

				// Append remaining_part
				url.Append(remaining_part);

				url_list->push_back(url);
			}

			return (url_list->size() > 0) ? origin : nullptr;
		}
	}

	return nullptr;
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const ov::String &name, info::Application *app_info)
{
	for (auto &app : _app_map)
	{
		if (app.second.GetName() == name)
		{
			// The application is exists
			return Result::Exists;
		}
	}

	info::Application new_app_info(GenerateApplicationId(), name, cfg::Application());

	if (app_info != nullptr)
	{
		*app_info = new_app_info;
	}

	return CreateApplicationInternal(new_app_info);
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const info::Application &app_info)
{
	std::vector<std::shared_ptr<OrchestratorModuleInterface>> created_list;

	auto name = app_info.GetName();

	logti("Trying to create an application: [%s]", app_info.GetName().CStr());

	{
		auto app_item = _app_map.find(app_info.GetId());

		if (app_item != _app_map.end())
		{
			logti("Application [%s] (%d) is exists", app_info.GetName().CStr(), app_info.GetId());
			return Result::Exists;
		}
	}

	// Notify modules of creation events
	bool succeeded = true;

	for (auto &module : _modules)
	{
		if (module.module->OnCreateApplication(app_info))
		{
			created_list.push_back(module.module);
		}
		else
		{
			logte("The module %p (%s) returns error while creating the application %s",
				  module.module.get(), GetOrchestratorModuleTypeName(module.module->GetModuleType()), app_info.GetName().CStr());
			succeeded = false;
			break;
		}
	}

	if (succeeded)
	{
		_app_map.emplace(app_info.GetId(), app_info);
		return Result::Succeeded;
	}

	// Rollback
	logtd("Something is wrong. Rollbacking...");

	for (auto &module : created_list)
	{
		if (module->OnDeleteApplication(app_info) == false)
		{
			logte("The module %p (%s) cannot be rolled back for application %s",
				  module.get(), GetOrchestratorModuleTypeName(module->GetModuleType()), app_info.GetName().CStr());

			// Ignore this error
		}
	}

	return Result::Failed;
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(info::application_id_t app_id)
{
	auto app = _app_map.find(app_id);

	if (app == _app_map.end())
	{
		logti("Application %d is not exists", app_id);
		return Result::NotExists;
	}

	auto app_info = app->second;

	logti("Trying to delete the application: [%s] (%u)", app_info.GetName().CStr(), app_info.GetId());

	// Notify modules of deletion events
	for (auto &module : _modules)
	{
		if (module.module->OnDeleteApplication(app_info) == false)
		{
			logte("The module %p (%s) returns error while deleting the application %s",
				  module.module.get(), GetOrchestratorModuleTypeName(module.module->GetModuleType()), app_info.GetName().CStr());

			// Ignore this error
		}
	}

	// TODO(dimiden): Need to find the app in _origin_map and delete it

	return Result::Succeeded;
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(const info::Application &app_info)
{
	return DeleteApplicationInternal(app_info.GetId());
}

Orchestrator::Result Orchestrator::CreateApplication(const cfg::Application &app_conf)
{
	std::lock_guard<__decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<__decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	info::Application app_info(GenerateApplicationId(), app_conf);

	return CreateApplicationInternal(app_info);
}

Orchestrator::Result Orchestrator::DeleteApplication(const info::Application &app_info)
{
	std::lock_guard<__decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<__decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	return DeleteApplicationInternal(app_info);
}

bool Orchestrator::RequestPullStreamForUrl(const std::shared_ptr<const ov::Url> &url)
{
	// TODO(dimiden): The part that creates an app using the origin map is considered to handle different Origin's apps,
	// But, this part is not considered

	auto source = url->Source();
	auto provider = GetProviderForScheme(url->Scheme());

	if (provider == nullptr)
	{
		logte("Could not find provider for URL: %s", source.CStr());
		return false;
	}

	auto provider_module = std::dynamic_pointer_cast<OrchestratorProviderModuleInterface>(provider);

	if (provider_module->CheckOriginAvailability({source}) == false)
	{
		logte("The URL is not available: %s", source.CStr());
		return false;
	}

	info::Application app_info;
	auto result = CreateApplicationInternal(url->App(), &app_info);

	if (result != Result::Failed)
	{
		// The application is created successfully (or already exists)
		if (provider_module->PullStream(app_info, url->Stream(), {source}))
		{
			// The stream pulled successfully
			return true;
		}
	}
	else
	{
		// Could not create the application
		return false;
	}

	// Rollback if failed
	if (result == Result::Succeeded)
	{
		// If there is no existing app and it is created, delete the app
		DeleteApplicationInternal(app_info.GetId());
	}

	return false;
}

bool Orchestrator::RequestPullStreamForLocation(const ov::String &location)
{
	ov::String app_name;
	ov::String stream_name;
	std::vector<ov::String> url_list;

	auto origin = GetUrlListForLocation(location, &app_name, &stream_name, &url_list);

	if (origin != nullptr)
	{
		auto provider = GetProviderForScheme(origin->scheme);

		if (provider == nullptr)
		{
			logte("Could not find provider for the location: %s", location.CStr());
			return false;
		}

		auto provider_module = std::dynamic_pointer_cast<OrchestratorProviderModuleInterface>(provider);

		if (provider_module->CheckOriginAvailability(url_list) == false)
		{
			logte("The URLs is not available: %s", ov::String::Join(url_list, ", "));
			return false;
		}

		auto item = _app_map.find(origin->app_id);
		info::Application app_info(GenerateApplicationId(), app_name, cfg::Application());
		Result result;

		if (item != _app_map.end())
		{
			app_info = item->second;
			result = Result::Exists;
		}
		else
		{
			// Create a new application
			result = CreateApplicationInternal(app_info);

			if (result == Result::Failed)
			{
				// Failed to create the application
				return false;
			}
		}

		if (provider_module->PullStream(app_info, stream_name, url_list) == false)
		{
			// Rollback if needed
			if (result == Result::Succeeded)
			{
				DeleteApplicationInternal(app_info);
			}
		}
		else
		{
			return true;
		}
	}

	logte("Could not find Origin for the location: %s", location.CStr());

	return false;
}

bool Orchestrator::RequestPullStream(const ov::String &url)
{
	std::lock_guard<__decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<__decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	auto parsed_url = ov::Url::Parse(url.CStr());

	if (parsed_url != nullptr)
	{
		// The URL has a scheme
		return RequestPullStreamForUrl(parsed_url);
	}
	else
	{
		// There is no scheme in the URL
		std::lock_guard<__decltype(_origin_map_mutex)> lock_guard_for_origin_map(_origin_map_mutex);

		return RequestPullStreamForLocation(url);
	}

	return false;
}

bool Orchestrator::RequestPullStream(const ov::String &application, const ov::String &stream)
{
	return RequestPullStream(ov::String::FormatString("/%s/%s", application.CStr(), stream.CStr()));
}
