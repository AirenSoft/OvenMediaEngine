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

bool Orchestrator::ApplyOriginMap(const std::vector<cfg::VirtualHost> &vhost_list)
{
	std::lock_guard<decltype(_domain_list_mutex)> lock_guard(_domain_list_mutex);
	bool result = true;

	// Mark all items as deleted
	for (auto &domain : _domain_list)
	{
		if (domain.state != ItemState::Applied)
		{
			logte("Invalid domain state: %d (Expected: %d)", domain.state, ItemState::Applied);
			OV_ASSERT2(false);
		}

		domain.state = ItemState::Delete;

		for (auto &origin_item : domain.origin_list)
		{
			// This is a safe operation because origin_list is managed by Orchestrator
			if (origin_item.state != ItemState::Applied)
			{
				logte("Invalid origin state: %d (Expected: %d)", origin_item.state, ItemState::Applied);
				OV_ASSERT2(false);
			}

			origin_item.state = ItemState::Delete;
		}
	}

	std::vector<Domain> added_domain_list;

	// Compare with existing values
	for (auto &vhost : vhost_list)
	{
		logtd("Trying to apply OriginMap for host %s", vhost.GetName().CStr());

		if (ProcessDomainList(&_domain_list, vhost, &added_domain_list))
		{
			if (added_domain_list.size() > 0)
			{
				std::move(added_domain_list.begin(), added_domain_list.end(), std::back_inserter(_domain_list));
			}
		}
		else
		{
			logte("Could not apply OriginMap for host %s", vhost.GetName().CStr());
			result = false;
		}
	}

	// Organize domain_list
	for (auto domain_item = _domain_list.begin(); domain_item != _domain_list.end();)
	{
		auto &origin_list = domain_item->origin_list;

		switch (domain_item->state)
		{
			case ItemState::Unknown:
			case ItemState::Applied:
				// This situation should never happen here
				OV_ASSERT2(false);
				++domain_item;
				continue;

			case ItemState::NotChanged:
				logtd("  - Domain is not changed: %s", domain_item->name.CStr());

				// Just need to mark as Applied
				for (auto &origin : origin_list)
				{
					origin.state = ItemState::Applied;
				}

				domain_item->state = ItemState::Applied;
				++domain_item;
				continue;

			case ItemState::New:
				logtd("  - Domain is created: %s", domain_item->name.CStr());
				// Nothing to do
				break;

			case ItemState::Changed:
				logtd("  - Domain is changed: %s", domain_item->name.CStr());

				break;

			case ItemState::Delete:
				logtd("  - Domain is deleted: %s", domain_item->name.CStr());
				domain_item = _domain_list.erase(domain_item);
				continue;
		}

		domain_item->state = ItemState::Applied;

		for (auto origin_item = origin_list.begin(); origin_item != origin_list.end();)
		{
			switch (origin_item->state)
			{
				case ItemState::Unknown:
				case ItemState::Applied:
					// This situation should never happen here
					OV_ASSERT2(false);
					++origin_item;
					continue;

				case ItemState::NotChanged:
					logtd("    - Origin is not changed: %s", origin_item->location.CStr());
					// Nothing to do
					break;

				case ItemState::New:
					logtd("    - Origin is created: %s", origin_item->location.CStr());
					// Nothing to do
					break;

				case ItemState::Changed:
					logtd("    - Origin is changed: %s", origin_item->location.CStr());

					break;

				case ItemState::Delete:
					logtd("    - Origin is deleted: %s", origin_item->location.CStr());
					origin_item = origin_list.erase(origin_item);
					continue;
			}

			origin_item->state = ItemState::Applied;
			++origin_item;
		}

		++domain_item;
	}

	logtd("All items are applied");

	return result;
}

bool Orchestrator::ProcessOriginList(const std::vector<Origin> &origin_list, const std::vector<Origin> &new_origin_list, std::vector<Origin> *added_origin_list) const
{
	bool is_equal = true;

	// Verify that the same item in origin_list as the one in new_origin_list
	for (auto &new_origin : new_origin_list)
	{
		bool found = false;
		auto &new_config = new_origin.origin_config;

		for (auto &origin : origin_list)
		{
			auto &old_config = origin.origin_config;

			if (
				// ItemState::Delete means "It is not processed"
				(origin.state == ItemState::Delete) &&
				(new_config.GetLocation() == old_config.GetLocation()))
			{
				auto &new_pass = new_config.GetPass();
				auto &old_pass = old_config.GetPass();

				if (old_pass.GetScheme() != new_pass.GetScheme())
				{
					logtd("Scheme does not the same: %s != %s", old_pass.GetScheme().CStr(), new_pass.GetScheme().CStr());
					origin.state = ItemState::Changed;
				}
				else
				{
					auto &first_url_list = new_pass.GetUrlList();
					auto &second_url_list = old_pass.GetUrlList();

					if (std::equal(
							first_url_list.begin(), first_url_list.end(), second_url_list.begin(),
							[](const cfg::Url &url1, const cfg::Url &url2) -> bool {
								bool result = url1.GetUrl() == url2.GetUrl();

								if (result == false)
								{
									logtd("URL does not the same: %s != %s", url1.GetUrl().CStr(), url2.GetUrl().CStr());
								}

								return result;
							}) == false)
					{
						origin.state = ItemState::Changed;
					}
				}

				found = true;
				origin.state = (origin.state == ItemState::Delete) ? ItemState::NotChanged : ItemState::Changed;

				break;
			}
		}

		if (found == false)
		{
			// new_origin is a new item
			added_origin_list->push_back(new_origin);
			is_equal = false;
		}
	}

	return is_equal;
}

bool Orchestrator::ProcessDomainList(std::vector<Domain> *domain_list, const cfg::VirtualHost &vhost, std::vector<Domain> *added_domain_list) const
{
	auto &vhost_name = vhost.GetName();
	auto &domain_config = vhost.GetDomain();

	auto new_origin_list = std::vector<Origin>();

	logtd("Generating OriginMap for host %s...", vhost.GetName().CStr());
	for (auto &origin_config : vhost.GetOrigins().GetOriginList())
	{
		new_origin_list.emplace_back(origin_config);
	}

	// Check for the new/changed items
	// TODO(dimiden): Is there a way to reduce the cost of O(n^2)?
	for (auto &domain_name : domain_config.GetNameList())
	{
		auto name = domain_name.GetName();
		bool found = false;

		std::vector<Origin> added_origin_list;

		for (auto &domain : *domain_list)
		{
			if (
				// ItemState::Delete means "It is not processed"
				(domain.state == ItemState::Delete) &&
				(domain.vhost_name == vhost_name) && ((domain.name == name)))
			{
				// This is a safe operation because origin_list is managed by Orchestrator
				if (ProcessOriginList(domain.origin_list, new_origin_list, &added_origin_list) == false)
				{
					// Mark as changed
					logtd("Origin list of the domain does not the same");
					domain.state = ItemState::Changed;

					if (added_origin_list.size() > 0)
					{
						std::move(added_origin_list.begin(), added_origin_list.end(), std::back_inserter(domain.origin_list));
					}
				}
				else
				{
					// Mark as not changed
					logtd("Origin list of the domain does the same");
					domain.state = ItemState::NotChanged;
				}

				found = true;
				break;
			}
			else
			{
				// does not matched
			}
		}

		if (found == false)
		{
			// domain_name is a new domain
			logtd("New domain found: %s", vhost_name.CStr());
			added_domain_list->emplace_back(vhost_name, name, new_origin_list);
		}
	}

	return true;
}

bool Orchestrator::RegisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
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

ov::String Orchestrator::ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name)
{
	// Replace all # to _
	return ov::String::FormatString("#%s#%s", vhost_name.Replace("#", "_").CStr(), app_name.Replace("#", "_").CStr());
}

ov::String Orchestrator::ResolveApplicationNameFromDomain(const ov::String &domain_name, const ov::String &app_name)
{
	auto vhost_name = GetVhostNameFromDomain(domain_name);

	if (vhost_name.IsEmpty())
	{
		logtw("Could not find VirtualHost for domain: %s", domain_name.CStr());
	}

	auto resolved = ResolveApplicationName(vhost_name, app_name);

	logtd("Resolved application name: %s (from domain: %s, app: %s)", resolved.CStr(), domain_name.CStr(), app_name.CStr());

	return std::move(resolved);
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

const Orchestrator::Origin &Orchestrator::GetUrlListForLocation(const ov::String &app_name, const ov::String &stream_name, std::vector<ov::String> *url_list)
{
	ov::String vhost_name;
	ov::String real_app_name;

	auto tokens = app_name.Split("#");

	if (tokens.size() == 3)
	{
		// #<vhost_name>#<app_name>
		OV_ASSERT2(tokens[0] == "");
		vhost_name = tokens[1];
		real_app_name = tokens[2];
	}
	else
	{
		// Invalid format
		OV_ASSERT2(false);
		real_app_name = tokens[0];
	}

	ov::String location = ov::String::FormatString("/%s/%s", real_app_name.CStr(), stream_name.CStr());

	logtd("Domains: %zu", _domain_list.size());

	// Find the origin using the location
	for (auto &domain : _domain_list)
	{
		if (domain.vhost_name != vhost_name)
		{
			// VHost does not matched
			continue;
		}

		logtd("OriginList for domain %s: %zu", domain.name.CStr(), domain.origin_list.size());

		for (auto &origin : domain.origin_list)
		{
			logtd("Trying to find the item that match location: %s", location.CStr());

			// TODO(dimien): Replace with the regex
			if (location.HasPrefix(origin.location))
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
				auto remaining_part = location.Substring(origin.location.GetLength());

				logtd("Found: location: %s (app: %s, stream: %s), remaining_part: %s", origin.location.CStr(), real_app_name.CStr(), stream_name.CStr(), remaining_part.CStr());

				for (auto url : origin.url_list)
				{
					// Append the remaining_part to the URL

					// For example,
					//    url:     ovt://origin.airensoft.com:9000/another_app/and_stream
					//    new_url: ovt://origin.airensoft.com:9000/another_app/and_stream_o
					//                                                                   ~~ <= remaining part

					// Prepend "<scheme>://"
					url.Prepend("://");
					url.Prepend(origin.scheme);

					// Append remaining_part
					url.Append(remaining_part);

					url_list->push_back(url);
				}

				return (url_list->size() > 0) ? origin : Origin::InvalidValue();
			}
		}
	}

	return Origin::InvalidValue();
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

bool Orchestrator::RequestPullStreamForLocation(const ov::String &app_name, const ov::String &stream_name, off_t offset)
{
	std::vector<ov::String> url_list;

	auto &origin = GetUrlListForLocation(app_name, stream_name, &url_list);

	if (origin.IsValid() == false)
	{
		logte("Could not find Origin for the stream: [%s/%s]", app_name.CStr(), stream_name.CStr());
		return false;
	}

	auto provider_module = GetProviderModuleForScheme(origin.scheme);

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

	auto stream = provider_module->PullStream(app_info, stream_name, url_list, offset);

	if (stream != nullptr)
	{
		// TODO(dimiden): Store the stream into the origin_list
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

bool Orchestrator::RequestPullStream(const ov::String &application, const ov::String &stream, off_t offset)
{
	std::lock_guard<decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
	std::lock_guard<decltype(_app_map_mutex)> lock_guard_for_app_map(_app_map_mutex);
	std::lock_guard<decltype(_domain_list_mutex)> lock_guard_for_domain_list(_domain_list_mutex);

	return RequestPullStreamForLocation(application, stream, offset);
}
