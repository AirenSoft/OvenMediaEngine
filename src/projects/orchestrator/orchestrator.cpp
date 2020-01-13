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

bool Orchestrator::ApplyOriginMap(const std::vector<cfg::VirtualHost> &host_list)
{
	bool result = true;

	for (auto &host : host_list)
	{
		logtd("Trying to apply OriginMap for host %s", host.GetName().CStr());

		if (ApplyOriginMap(host.GetName(), host.GetDomain(), host.GetOrigins()) == false)
		{
			logte("Could not apply OriginMap for host %s", host.GetName().CStr());
			result = false;
		}
	}

	return result;
}

bool Orchestrator::ApplyOriginMap(const ov::String &vhost_name, const cfg::Domain &domain_config, const cfg::Origins &origins_config)
{
	std::lock_guard<decltype(_domain_list_mutex)> lock_guard(_domain_list_mutex);
	decltype(_domain_list) old_domain_list = std::move(_domain_list);

	// Mark all items as deleted
	for (auto &old_domain : old_domain_list)
	{
		OV_ASSERT(old_domain.state == DomainItemState::Applied, "%s: %d", old_domain.name.CStr(), old_domain.state);
		old_domain.state = DomainItemState::Delete;
	}

	auto new_origin_list = std::vector<std::shared_ptr<const Origin>>();

	logtd("Generating OriginList...");
	for (auto &origin : origins_config.GetOriginList())
	{
		new_origin_list.emplace_back(std::make_shared<const Origin>(origin));
	}

	// Check for the new/modified items
	// TODO(dimiden): Is there a way to reduce the cost of O(n^2)?
	for (auto &domain_name : domain_config.GetNameList())
	{
		bool found = false;
		auto name = domain_name.GetName();

		auto old_item = old_domain_list.begin();
		for (; old_item != old_domain_list.end(); ++old_item)
		{
			if ((old_item->state == DomainItemState::Delete) && (old_item->name == name))
			{
				auto old_origin_list = old_item->origin_list;

				// TODO(dimiden): Need to pick out the changed "Origin" here
				if (old_origin_list.size() != new_origin_list.size())
				{
					logtd("Size does not the same: %zu, %zu", old_origin_list.size(), new_origin_list.size());
					old_item->state = DomainItemState::Changed;
				}
				else
				{
					if (std::equal(
							old_origin_list.begin(), old_origin_list.end(), new_origin_list.begin(),
							[](const std::shared_ptr<const Origin> &origin1, const std::shared_ptr<const Origin> &origin2) -> bool {
								return origin1->operator==(origin2.get());
							}))
					{
						logtd("OriginList does the same");
						old_item->state = DomainItemState::NotChanged;

						_domain_list.push_back(*old_item);
						old_domain_list.erase(old_item);
						found = true;
					}
					else
					{
						logtd("OriginList does not the same");
						old_item->state = DomainItemState::Changed;
					}
				}

				break;
			}
		}

		if (found == false)
		{
			// Add the item if not found
			_domain_list.emplace_back(vhost_name, name, new_origin_list);

			if (old_item != old_domain_list.end())
			{
				old_domain_list.erase(old_item);
			}
		}
	}

	for (auto &domain_item : _domain_list)
	{
		switch (domain_item.state)
		{
			case DomainItemState::Unknown:
			case DomainItemState::Applied:
			case DomainItemState::Delete:
				// This situation should never happen here
				OV_ASSERT2(false);
				break;

			case DomainItemState::NotChanged:
				logtd("%s is not changed", domain_item.name.CStr());
				break;

			case DomainItemState::New:
				logtd("%s is new domain", domain_item.name.CStr());
				break;

			case DomainItemState::Changed:
				logtd("%s is changed", domain_item.name.CStr());
				break;
		}

		domain_item.state = DomainItemState::Applied;
	}

	for (auto &deleted_domain_item : old_domain_list)
	{
		if (deleted_domain_item.state == DomainItemState::Delete)
		{
			logtd("%s is deleted", deleted_domain_item.name.CStr());
		}
		else
		{
			OV_ASSERT2(false);
		}
	}

	return true;
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
	std::lock_guard<decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto &info : _modules)
	{
		if (info.module == module)
		{
			if (info.type == type)
			{
				logtw("%s module (%p) is already registered", GetOrchestratorModuleTypeName(type), module.get());
			}
			else
			{
				logtw("The module type was %s (%p), but now %s", GetOrchestratorModuleTypeName(info.type), module.get(), GetOrchestratorModuleTypeName(type));
			}

			OV_ASSERT2(false);
			return false;
		}
	}

	_modules.emplace_back(type, module);
	auto &list = _module_map[type];
	list.push_back(module);

	logtd("%s module (%p) is registered", GetOrchestratorModuleTypeName(type), module.get());

	return true;
}

bool Orchestrator::UnregisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		OV_ASSERT2(module != nullptr);
		return false;
	}

	std::lock_guard<decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto info = _modules.begin(); info != _modules.end(); ++info)
	{
		if (info->module == module)
		{
			_modules.erase(info);
			auto &list = _module_map[info->type];
			logtd("%s module (%p) is unregistered", GetOrchestratorModuleTypeName(info->type), module.get());
			return true;
		}
	}

	logtw("%s module (%p) not found", GetOrchestratorModuleTypeName(module->GetModuleType()), module.get());
	OV_ASSERT2(false);

	return false;
}

ov::String Orchestrator::ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name)
{
	ov::String new_app_name;

	if (vhost_name.IsEmpty() == false)
	{
		// Replace all # to _
		new_app_name = ov::String::FormatString("#%s#%s", vhost_name.Replace("#", "_").CStr(), app_name.Replace("#", "_").CStr());
	}
	else
	{
		new_app_name = ov::String::FormatString("#%s", app_name.Replace("#", "_").CStr());
	}

	logtd("Resolved application name: %s (from host: %s, app: %s)", new_app_name.CStr(), vhost_name.CStr(), app_name.CStr());

	return std::move(new_app_name);
}

ov::String Orchestrator::GetVhostNameFromDomain(const ov::String &domain_name)
{
	std::lock_guard<decltype(_domain_list_mutex)> lock_guard(_domain_list_mutex);

	if (domain_name.IsEmpty() == false)
	{
		// Search for the domain corresponding to domain_name
		for (auto &domain_item : _domain_list)
		{
			if (std::regex_match(domain_name.CStr(), domain_item.regex_for_domain))
			{
				return domain_item.vhost_name;
			}
		}
	}

	return "";
}

info::application_id_t Orchestrator::GetNextAppId()
{
	while (true)
	{
		_last_application_id++;

		if (_last_application_id == info::MaxApplicationId)
		{
			_last_application_id = info::MinApplicationId;
		}

		if (_app_map.find(_last_application_id) == _app_map.end())
		{
			return _last_application_id;
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
				OV_ASSERT(provider != nullptr, "Provider must inherit from pvd::Provider");
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

std::shared_ptr<OrchestratorProviderModuleInterface> Orchestrator::GetProviderModuleForScheme(const ov::String &scheme)
{
	auto provider = GetProviderForScheme(scheme);
	auto provider_module = std::dynamic_pointer_cast<OrchestratorProviderModuleInterface>(provider);

	OV_ASSERT((provider == nullptr) || (provider_module != nullptr),
			  "Provider (%d) shouldmust inherit from OrchestratorProviderModuleInterface",
			  provider->GetProviderType());

	return provider_module;
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

std::shared_ptr<const Orchestrator::Origin> Orchestrator::GetUrlListForLocation(const ov::String &app_name, const ov::String &stream_name, std::vector<ov::String> *url_list)
{
	auto tokens = app_name.Split("#");
	ov::String host_name;
	ov::String real_app_name;

	switch (tokens.size())
	{
		case 1:
		default:
			// app only - this is a bug
			// app_name must always start with a "#"
			OV_ASSERT2(false);
			real_app_name = tokens[0];
			break;

		case 2:
			// default host & app_name
			// #<app_name>
			host_name = tokens[0];
			OV_ASSERT2(host_name == "");
			real_app_name = tokens[1];
			break;

		case 3:
			// domain & app_name
			// #<domain>#<app_name>
			OV_ASSERT2(tokens[0] == "");
			host_name = tokens[1];
			real_app_name = tokens[2];
			break;
	}

	ov::String location = ov::String::FormatString("/%s/%s", real_app_name.CStr(), stream_name.CStr());

	logtd("Domains: %zu", _domain_list.size());

	// Find the origin using the location
	for (auto &domain : _domain_list)
	{
		logtd("OriginList for domain %s: %zu", domain.name.CStr(), domain.origin_list.size());

		for (auto &origin : domain.origin_list)
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
				//              <Scheme>ovt</Scheme>
				//      		<Url>origin.airensoft.com:9000/another_app/and_stream</Url>
				//      	</Pass>
				//      </Origin>
				// And when the location is "/app/stream_o",
				//
				// <Location>: /app/stream
				// location:   /app/stream_o
				//                        ~~ <= remaining part
				auto remaining_part = location.Substring(origin->location.GetLength());

				logtd("Found: location: %s (app: %s, stream: %s), remaining_part: %s", origin->location.CStr(), real_app_name.CStr(), stream_name.CStr(), remaining_part.CStr());

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
	}

	return nullptr;
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const info::Application &app_info)
{
	auto &app_name = app_info.GetName();

	for (auto &app : _app_map)
	{
		if (app.second.GetName() == app_name)
		{
			// The application does exists
			return Result::Exists;
		}
	}

	logti("Trying to create an application: [%s]", app_name.CStr());

	// Notify modules of creation events
	std::vector<std::shared_ptr<OrchestratorModuleInterface>> created_list;
	bool succeeded = true;

	_app_map.emplace(app_info.GetId(), app_info);

	for (auto &module : _modules)
	{
		if (module.module->OnCreateApplication(app_info))
		{
			created_list.push_back(module.module);
		}
		else
		{
			logte("The module %p (%s) returns error while creating the application [%s]",
				  module.module.get(), GetOrchestratorModuleTypeName(module.module->GetModuleType()), app_name.CStr());
			succeeded = false;
			break;
		}
	}

	if (succeeded)
	{
		return Result::Succeeded;
	}

	logte("Trying to rollback for the application [%s]", app_name.CStr());
	return DeleteApplicationInternal(app_info);
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const ov::String &app_name, info::Application *app_info)
{
	OV_ASSERT2(app_info != nullptr);

	*app_info = info::Application(GetNextAppId(), app_name);

	return CreateApplicationInternal(*app_info);
}

Orchestrator::Result Orchestrator::NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info)
{
	Result result = Result::Succeeded;

	// Notify modules of deletion events
	for (auto &module : modules)
	{
		if (module.module->OnDeleteApplication(app_info) == false)
		{
			logte("The module %p (%s) returns error while deleting the application %s",
				  module.module.get(), GetOrchestratorModuleTypeName(module.module->GetModuleType()), app_info.GetName().CStr());

			// Ignore this error
			result = Result::Failed;
		}
	}

	return result;
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(info::application_id_t app_id)
{
	auto app = _app_map.find(app_id);

	if (app == _app_map.end())
	{
		logti("Application %d does not exists", app_id);
		return Result::NotExists;
	}

	auto app_info = app->second;

	logti("Trying to delete the application: [%s] (%u)", app_info.GetName().CStr(), app_info.GetId());

	_app_map.erase(app_id);

	return NotifyModulesForDeleteEvent(_modules, app_info);
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(const info::Application &app_info)
{
	return DeleteApplicationInternal(app_info.GetId());
}

Orchestrator::Result Orchestrator::CreateApplication(const ov::String &vhost_name, const cfg::Application &app_config)
{
	std::lock_guard<decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	info::Application app_info(GetNextAppId(), ResolveApplicationName(vhost_name, app_config.GetName()), app_config);

	return CreateApplicationInternal(app_info);
}

Orchestrator::Result Orchestrator::DeleteApplication(const info::Application &app_info)
{
	std::lock_guard<decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	return DeleteApplicationInternal(app_info);
}

const info::Application &Orchestrator::GetApplicationInternal(const ov::String &app_name) const
{
	for (auto &app : _app_map)
	{
		if (app.second.GetName() == app_name)
		{
			return app.second;
		}
	}

	return info::Application::GetInvalidApplication();
}

const info::Application &Orchestrator::GetApplication(const ov::String &app_name) const
{
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	return GetApplicationInternal(app_name);
}

const info::Application &Orchestrator::GetApplicationInternal(info::application_id_t app_id) const
{
	auto app = _app_map.find(app_id);

	if (app != _app_map.end())
	{
		return app->second;
	}

	return info::Application::GetInvalidApplication();
}

const info::Application &Orchestrator::GetApplication(info::application_id_t app_id) const
{
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	return GetApplicationInternal(app_id);
}

#if 0
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

bool Orchestrator::RequestPullStream(const ov::String &url)
{
	std::lock_guard<decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);

	auto parsed_url = ov::Url::Parse(url.CStr());

	if (parsed_url != nullptr)
	{
		// The URL has a scheme
		return RequestPullStreamForUrl(parsed_url);
	}
	else
	{
		// Invalid URL
		logte("Pull stream is requested for invalid URL: %s", url.CStr());
	}

	return false;
}
#endif

bool Orchestrator::RequestPullStreamForLocation(const ov::String &app_name, const ov::String &stream_name)
{
	std::vector<ov::String> url_list;

	auto origin = GetUrlListForLocation(app_name, stream_name, &url_list);

	if (origin == nullptr)
	{
		logte("Could not find Origin for the stream: [%s/%s]", app_name.CStr(), stream_name.CStr());
		return false;
	}

	auto provider_module = GetProviderModuleForScheme(origin->scheme);

	if (provider_module == nullptr)
	{
		logte("Could not find provider for the stream: [%s/%s]", app_name.CStr(), stream_name.CStr());
		return false;
	}

	// Check if the application does exists
	auto app_info = GetApplicationInternal(app_name);
	Result result;

	if (app_info.IsValid())
	{
		result = Result::Exists;
	}
	else
	{
		// Create a new application
		result = CreateApplicationInternal(app_name, &app_info);

		if (
			// Failed to create the application
			(result == Result::Failed) ||
			// result always must be Result::Succeeded
			(result != Result::Succeeded))
		{
			return false;
		}
	}

	logti("Trying to pull stream [%s/%s] from provider: %s", app_name.CStr(), stream_name.CStr(), GetOrchestratorModuleTypeName(provider_module->GetModuleType()));

	if (provider_module->PullStream(app_info, stream_name, url_list))
	{
		logti("The stream was pulled successfully: [%s/%s]", app_name.CStr(), stream_name.CStr());
		return true;
	}

	logte("Could not pull stream [%s/%s] from provider: %s", app_name.CStr(), stream_name.CStr(), GetOrchestratorModuleTypeName(provider_module->GetModuleType()));
	// Rollback if needed

	switch (result)
	{
		case Result::Failed:
		case Result::NotExists:
			// This is a bug - Must be handled above
			OV_ASSERT2(false);
			break;

		case Result::Succeeded:
			// New application is created. Rollback is required
			DeleteApplicationInternal(app_info);
			break;

		case Result::Exists:
			// Used a previously created application. Do not need to rollback
			break;
	}

	return false;
}

bool Orchestrator::RequestPullStream(const ov::String &application, const ov::String &stream)
{
	std::lock_guard<decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);
	std::lock_guard<decltype(_domain_list_mutex)> lock_guard_for_domain_list(_domain_list_mutex);

	return RequestPullStreamForLocation(application, stream);
}
