//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "orchestrator.h"

#include <base/mediarouter/media_route_interface.h>
#include <base/provider/stream.h>
#include <mediarouter/mediarouter.h>
#include <monitoring/monitoring.h>

#include "orchestrator_private.h"

bool Orchestrator::ApplyForVirtualHost(const std::shared_ptr<VirtualHost> &virtual_host)
{
	auto succeeded = true;

	logtd("Trying to apply new configuration of VirtualHost: %s...", virtual_host->name.CStr());

	if (virtual_host->state == ItemState::Delete)
	{
		logtd("VirtualHost is deleted");

		// Delete all apps that were created by this VirtualHost
		for (auto app_item : virtual_host->app_map)
		{
			auto &app_info = app_item.second->app_info;

			auto result = DeleteApplicationInternal(app_info);

			if (result != Result::Succeeded)
			{
				logte("Could not delete application: %s", app_info.GetName().CStr());
				succeeded = false;
			}
		}
	}
	else
	{
		logtd("VirtualHost is changed");

		// Check if there is a deleted host, and then check if there are any apps created by that host.
		auto &host_list = virtual_host->host_list;

		for (auto host_item = host_list.begin(); host_item != host_list.end();)
		{
			switch (host_item->state)
			{
				default:
					// This situation should never happen here
					OV_ASSERT2(false);
					logte("Invalid domain state: %s, %d", host_item->name.CStr(), host_item->state);
					++host_item;
					break;

				case ItemState::Applied:
				case ItemState::NotChanged:
				case ItemState::New:
					// Nothing to do
					logtd("Domain is not changed/just created: %s", host_item->name.CStr());
					++host_item;
					break;

				case ItemState::NeedToCheck:
					// This item was deleted because it was never processed in the above process
				case ItemState::Changed:
				case ItemState::Delete:
					logtd("Domain is changed/deleted: %s", host_item->name.CStr());
					for (auto &actual_host_item : host_item->stream_map)
					{
						auto &stream_list = actual_host_item.second;

						for (auto &stream_item : stream_list)
						{
							auto &stream = stream_item.second;

							if (stream->is_valid)
							{
								logti("Trying to stop stream [%s]...", stream->full_name.CStr());

								if (stream->provider->StopStream(stream->app_info, stream->provider_stream) == false)
								{
									logte("Failed to stop stream [%s] in provider: %s", stream->full_name.CStr(), GetOrchestratorModuleTypeName(stream->provider->GetModuleType()).CStr());
								}

								stream->is_valid = false;
							}
						}
					}

					host_item = host_list.erase(host_item);
					break;
			}
		}

		// Check if there is a deleted origin, and then check if there are any apps created by that origin.
		auto &origin_list = virtual_host->origin_list;

		for (auto origin_item = origin_list.begin(); origin_item != origin_list.end();)
		{
			switch (origin_item->state)
			{
				default:
					// This situation should never happen here
					logte("Invalid origin state: %s, %d", origin_item->location.CStr(), origin_item->state);
					++origin_item;
					break;

				case ItemState::Applied:
				case ItemState::NotChanged:
				case ItemState::New:
					// Nothing to do
					logtd("Origin is not changed/just created: %s", origin_item->location.CStr());
					++origin_item;
					break;

				case ItemState::NeedToCheck:
					// This item was deleted because it was never processed in the above process
				case ItemState::Changed:
				case ItemState::Delete:
					logtd("Origin is changed/deleted: %s", origin_item->location.CStr());
					for (auto &stream_item : origin_item->stream_map)
					{
						auto &stream = stream_item.second;

						if (stream->is_valid)
						{
							logti("Trying to stop stream [%s]...", stream->full_name.CStr());

							if (stream->provider->StopStream(stream->app_info, stream->provider_stream) == false)
							{
								logte("Failed to stop stream [%s] in provider: %s", stream->full_name.CStr(), GetOrchestratorModuleTypeName(stream->provider->GetModuleType()).CStr());
							}

							stream->is_valid = false;
						}
					}

					origin_item = origin_list.erase(origin_item);
					break;
			}
		}
	}

	return succeeded;
}

bool Orchestrator::ApplyOriginMap(const std::vector<info::Host> &host_list)
{
	bool result = true;
	auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

	// Mark all items as NeedToCheck
	for (auto &vhost_item : _virtual_host_map)
	{
		if (vhost_item.second->MarkAllAs(ItemState::Applied, ItemState::NeedToCheck) == false)
		{
			logtd("Something was wrong with VirtualHost: %s", vhost_item.second->name.CStr());

			OV_ASSERT2(false);
			result = false;
		}
	}

	logtd("- Processing for VirtualHosts");

	for (auto &host_info : host_list)
	{
		auto previous_vhost_item = _virtual_host_map.find(host_info.GetName());

		if (previous_vhost_item == _virtual_host_map.end())
		{
			logtd("  - %s: New", host_info.GetName().CStr());
			auto vhost = std::make_shared<VirtualHost>(host_info);

			vhost->name = host_info.GetName();

			logtd("    - Processing for domains: %d items", host_info.GetDomain().GetNameList().size());

			for (auto &domain_name : host_info.GetDomain().GetNameList())
			{
				logtd("      - %s: New", domain_name.GetName().CStr());
				vhost->host_list.emplace_back(domain_name.GetName());
			}

			logtd("    - Processing for origins: %d items", host_info.GetOriginList().size());

			for (auto &origin_config : host_info.GetOriginList())
			{
				logtd("      - %s: New (%zu urls)",
					  origin_config.GetLocation().CStr(),
					  origin_config.GetPass().GetUrlList().size());

				vhost->origin_list.emplace_back(origin_config);
			}

			_virtual_host_map[host_info.GetName()] = vhost;
			_virtual_host_list.push_back(vhost);

			continue;
		}

		logtd("  - %s: Not changed", host_info.GetName().CStr());

		// Check the previous VirtualHost item
		auto &vhost = previous_vhost_item->second;

		logtd("    - Processing for hosts");
		auto new_state_for_domain = ProcessHostList(&(vhost->host_list), host_info.GetDomain());
		logtd("    - Processing for origins");
		auto new_state_for_origin = ProcessOriginList(&(vhost->origin_list), host_info.GetOrigins());

		if ((new_state_for_domain == ItemState::NotChanged) && (new_state_for_origin == ItemState::NotChanged))
		{
			vhost->state = ItemState::NotChanged;
		}
		else
		{
			vhost->state = ItemState::Changed;
		}
	}

	// Dump all items
	for (auto vhost_item = _virtual_host_list.begin(); vhost_item != _virtual_host_list.end();)
	{
		auto &vhost = *vhost_item;

		switch (vhost->state)
		{
			case ItemState::Unknown:
			case ItemState::Applied:
			case ItemState::Delete:
			default:
				// This situation should never happen here
				logte("  - %s: Invalid VirtualHost state: %d", vhost->name.CStr(), vhost->state);
				result = false;
				OV_ASSERT2(false);

				// Delete the invalid item
				vhost_item = _virtual_host_list.erase(vhost_item);
				_virtual_host_map.erase(vhost->name);

				vhost->MarkAllAs(ItemState::Delete);
				ApplyForVirtualHost(vhost);

				break;

			case ItemState::NeedToCheck:
				// This item was deleted because it was never processed in the above process
				logtd("  - %s: Deleted", vhost->name.CStr());

				vhost_item = _virtual_host_list.erase(vhost_item);
				_virtual_host_map.erase(vhost->name);

				vhost->MarkAllAs(ItemState::Delete);
				ApplyForVirtualHost(vhost);

				break;

			case ItemState::NotChanged:
			case ItemState::New:
				// Nothing to do
				vhost->MarkAllAs(ItemState::Applied);
				++vhost_item;
				break;

			case ItemState::Changed:
				++vhost_item;

				if (ApplyForVirtualHost(vhost))
				{
					vhost->MarkAllAs(ItemState::Applied);
				}

				break;
		}
	}

	logtd("All items are applied");

	return result;
}

const std::vector<std::shared_ptr<Orchestrator::VirtualHost>> &Orchestrator::GetVirtualHostList()
{
	return _virtual_host_list;
}

Orchestrator::ItemState Orchestrator::ProcessHostList(std::vector<Host> *domain_list, const cfg::Domain &domain_config) const
{
	bool is_changed = false;

	// TODO(dimiden): Is there a way to reduce the cost of O(n^2)?
	for (auto &domain_name : domain_config.GetNameList())
	{
		auto name = domain_name.GetName();
		bool found = false;

		for (auto &domain : *domain_list)
		{
			if (domain.state == ItemState::NeedToCheck)
			{
				if (domain.name == name)
				{
					domain.state = ItemState::NotChanged;
					found = true;
					break;
				}
			}
		}

		if (found == false)
		{
			logtd("      - %s: New", domain_name.GetName().CStr());
			// Adding items here causes unnecessary iteration in the for statement above
			// To avoid this, we need to create a separate list for each added item
			domain_list->push_back(name);
			is_changed = true;
		}
	}

	if (is_changed == false)
	{
		// There was no new item

		// Check for deleted items
		for (auto &domain : *domain_list)
		{
			switch (domain.state)
			{
				case ItemState::NeedToCheck:
					// This item was deleted because it was never processed in the above process
					logtd("      - %s: Deleted", domain.name.CStr());
					domain.state = ItemState::Delete;
					is_changed = true;
					break;

				case ItemState::NotChanged:
					// Nothing to do
					logtd("      - %s: Not changed", domain.name.CStr());
					break;

				case ItemState::Unknown:
				case ItemState::Applied:
				case ItemState::Changed:
				case ItemState::New:
				case ItemState::Delete:
				default:
					// This situation should never happen here
					OV_ASSERT2(false);
					is_changed = true;
					break;
			}
		}
	}

	return is_changed ? ItemState::Changed : ItemState::NotChanged;
}

Orchestrator::ItemState Orchestrator::ProcessOriginList(std::vector<Origin> *origin_list, const cfg::Origins &origins_config) const
{
	bool is_changed = false;

	// TODO(dimiden): Is there a way to reduce the cost of O(n^2)?
	for (auto &origin_config : origins_config.GetOriginList())
	{
		bool found = false;

		for (auto &origin : *origin_list)
		{
			if (origin.state == ItemState::NeedToCheck)
			{
				if (origin.location == origin_config.GetLocation())
				{
					auto &prev_pass_config = origin.origin_config.GetPass();
					auto &new_pass_config = origin_config.GetPass();

					if (prev_pass_config.GetScheme() != new_pass_config.GetScheme())
					{
						logtd("      - %s: Changed (Scheme does not the same: %s != %s)", origin.location.CStr(), prev_pass_config.GetScheme().CStr(), new_pass_config.GetScheme().CStr());
						origin.state = ItemState::Changed;
					}
					else
					{
						auto &first_url_list = new_pass_config.GetUrlList();
						auto &second_url_list = prev_pass_config.GetUrlList();

						bool is_equal = std::equal(
							first_url_list.begin(), first_url_list.end(), second_url_list.begin(),
							[&origin](const cfg::Url &url1, const cfg::Url &url2) -> bool {
								bool result = url1.GetUrl() == url2.GetUrl();

								if (result == false)
								{
									logtd("      - %s: Changed (URL does not the same: %s != %s)", origin.location.CStr(), url1.GetUrl().CStr(), url2.GetUrl().CStr());
								}

								return result;
							});

						origin.state = (is_equal == false) ? ItemState::Changed : ItemState::NotChanged;
					}

					if (origin.state == ItemState::Changed)
					{
						is_changed = true;
					}

					found = true;
					break;
				}
			}
		}

		if (found == false)
		{
			// Adding items here causes unnecessary iteration in the for statement above
			// To avoid this, we need to create a separate list for each added item
			logtd("      - %s: New (%zd urls)", origin_config.GetLocation().CStr(), origin_config.GetPass().GetUrlList().size());
			origin_list->push_back(origin_config);
			is_changed = true;
		}
	}

	if (is_changed == false)
	{
		// There was no new item

		// Check for deleted items
		for (auto &origin : *origin_list)
		{
			switch (origin.state)
			{
				case ItemState::NeedToCheck:
					// This item was deleted because it was never processed in the above process
					logtd("      - %s: Deleted", origin.location.CStr());
					origin.state = ItemState::Delete;
					is_changed = true;
					break;

				case ItemState::NotChanged:
					logtd("      - %s: Not changed (%zu)", origin.location.CStr(), origin.url_list.size());
					// Nothing to do
					break;

				case ItemState::Unknown:
				case ItemState::Applied:
				case ItemState::Changed:
				case ItemState::New:
				case ItemState::Delete:
				default:
					// This situation should never happen here
					OV_ASSERT2(false);
					is_changed = true;
					break;
			}
		}
	}

	return is_changed ? ItemState::Changed : ItemState::NotChanged;
}

bool Orchestrator::RegisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		return false;
	}

	auto type = module->GetModuleType();

	// Check if module exists
	auto scoped_lock = std::scoped_lock(_module_list_mutex);

	for (auto &info : _module_list)
	{
		if (info.module == module)
		{
			if (info.type == type)
			{
				logtw("%s module (%p) is already registered", GetOrchestratorModuleTypeName(type).CStr(), module.get());
			}
			else
			{
				logtw("The module type was %s (%p), but now %s", GetOrchestratorModuleTypeName(info.type).CStr(), module.get(), GetOrchestratorModuleTypeName(type).CStr());
			}

			OV_ASSERT2(false);
			return false;
		}
	}

	_module_list.emplace_back(type, module);

	if (module->GetModuleType() == OrchestratorModuleType::MediaRouter)
	{
		auto media_router = std::dynamic_pointer_cast<MediaRouter>(module);

		OV_ASSERT2(media_router != nullptr);

		_media_router = media_router;
	}

	logtd("%s module (%p) is registered", GetOrchestratorModuleTypeName(type).CStr(), module.get());

	return true;
}

bool Orchestrator::UnregisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		OV_ASSERT2(module != nullptr);
		return false;
	}

	auto scoped_lock = std::scoped_lock(_module_list_mutex);

	for (auto info = _module_list.begin(); info != _module_list.end(); ++info)
	{
		if (info->module == module)
		{
			_module_list.erase(info);
			logtd("%s module (%p) is unregistered", GetOrchestratorModuleTypeName(info->type).CStr(), module.get());
			return true;
		}
	}

	logtw("%s module (%p) not found", GetOrchestratorModuleTypeName(module->GetModuleType()).CStr(), module.get());
	OV_ASSERT2(false);

	return false;
}

ov::String Orchestrator::GetVhostNameFromDomain(const ov::String &domain_name)
{
	// TODO(dimiden): I think it would be nice to create a VHost cache for performance

	if (domain_name.IsEmpty() == false)
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		// Search for the domain corresponding to domain_name

		// CAUTION: This code is important to order, so don't use _virtual_host_map
		for (auto &vhost_item : _virtual_host_list)
		{
			for (auto &host_item : vhost_item->host_list)
			{
				if (std::regex_match(domain_name.CStr(), host_item.regex_for_domain))
				{
					return vhost_item->name;
				}
			}
		}
	}

	return "";
}

info::VHostAppName Orchestrator::ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name) const
{
	// Replace all # to _
	return info::VHostAppName(ov::String::FormatString("#%s#%s", vhost_name.Replace("#", "_").CStr(), app_name.Replace("#", "_").CStr()));
}

info::VHostAppName Orchestrator::ResolveApplicationNameFromDomain(const ov::String &domain_name, const ov::String &app_name)
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
	return _last_application_id++;
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
		type = ProviderType::RtspPull;
	}
	else if (lower_scheme == "rtspc")
	{
		type = ProviderType::RtspPull;
	}
	else if (lower_scheme == "ovt")
	{
		type = ProviderType::Ovt;
	}
	else
	{
		logte("Could not find a provider for scheme [%s]", scheme.CStr());
		return nullptr;
	}

	// Find the provider
	std::shared_ptr<OrchestratorPullProviderModuleInterface> module = nullptr;

	for (auto info = _module_list.begin(); info != _module_list.end(); ++info)
	{
		if (info->type == OrchestratorModuleType::PullProvider)
		{
			auto module = std::dynamic_pointer_cast<OrchestratorPullProviderModuleInterface>(info->module);
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

std::shared_ptr<OrchestratorPullProviderModuleInterface> Orchestrator::GetProviderModuleForScheme(const ov::String &scheme)
{
	auto provider = GetProviderForScheme(scheme);
	auto provider_module = std::dynamic_pointer_cast<OrchestratorPullProviderModuleInterface>(provider);

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

bool Orchestrator::ParseVHostAppName(const info::VHostAppName &vhost_app_name, ov::String *vhost_name, ov::String *real_app_name) const
{
	auto tokens = vhost_app_name.ToString().Split("#");

	if (tokens.size() == 3)
	{
		// #<vhost_name>#<app_name>
		OV_ASSERT2(tokens[0] == "");

		if (vhost_name != nullptr)
		{
			*vhost_name = tokens[1];
		}

		if (real_app_name != nullptr)
		{
			*real_app_name = tokens[2];
		}
		return true;
	}

	OV_ASSERT2(false);
	return false;
}

std::shared_ptr<Orchestrator::VirtualHost> Orchestrator::GetVirtualHost(const ov::String &vhost_name)
{
	auto vhost_item = _virtual_host_map.find(vhost_name);

	if (vhost_item == _virtual_host_map.end())
	{
		return nullptr;
	}

	return vhost_item->second;
}

std::shared_ptr<const Orchestrator::VirtualHost> Orchestrator::GetVirtualHost(const ov::String &vhost_name) const
{
	auto vhost_item = _virtual_host_map.find(vhost_name);

	if (vhost_item == _virtual_host_map.end())
	{
		return nullptr;
	}

	return vhost_item->second;
}

std::shared_ptr<Orchestrator::VirtualHost> Orchestrator::GetVirtualHost(const info::VHostAppName &vhost_app_name, ov::String *real_app_name)
{
	ov::String vhost_name;

	if (ParseVHostAppName(vhost_app_name, &vhost_name, real_app_name))
	{
		return GetVirtualHost(vhost_name);
	}

	// Invalid format
	OV_ASSERT2(false);
	return nullptr;
}

std::shared_ptr<const Orchestrator::VirtualHost> Orchestrator::GetVirtualHost(const info::VHostAppName &vhost_app_name, ov::String *real_app_name) const
{
	ov::String vhost_name;

	if (ParseVHostAppName(vhost_app_name, &vhost_name, real_app_name))
	{
		return GetVirtualHost(vhost_name);
	}

	// Invalid format
	OV_ASSERT2(false);
	return nullptr;
}

bool Orchestrator::GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list)
{
	auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

	return GetUrlListForLocationInternal(vhost_app_name, host_name, stream_name, url_list, nullptr, nullptr);
}

bool Orchestrator::GetUrlListForLocationInternal(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list, Origin **matched_origin, Host **matched_host)
{
	ov::String real_app_name;
	auto vhost = GetVirtualHost(vhost_app_name, &real_app_name);

	if (vhost == nullptr)
	{
		logte("Could not find VirtualHost for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
		return false;
	}

	Host *found_matched_host = nullptr;
	Origin *found_matched_origin = nullptr;

	auto &host_list = vhost->host_list;
	auto &origin_list = vhost->origin_list;

	ov::String location = ov::String::FormatString("/%s/%s", real_app_name.CStr(), stream_name.CStr());

	// Find the host using the location
	for (auto &host : host_list)
	{
		logtd("Trying to find the item from host_list that match host_name: %s", host_name.CStr());

		if (std::regex_match(host_name.CStr(), host.regex_for_domain))
		{
			found_matched_host = &host;
			break;
		}
	}

	if (found_matched_host == nullptr)
	{
		logtd("Could not find host for %s", host_name.CStr());
		return false;
	}

	// Find the origin using the location
	for (auto &origin : origin_list)
	{
		logtd("Trying to find the item from origin_list that match location: %s", location.CStr());

		// TODO(dimien): Replace with the regex
		if (location.HasPrefix(origin.location))
		{
			// If the location has the prefix that configured in <Origins>, extract the remaining part
			// For example, if the settings is:
			//      <Origin>
			//      	<Location>/app/stream</Location>
			//      	<Pass>
			//              <Scheme>ovt</Scheme>
			//              <Urls>
			//      		    <Url>origin.airensoft.com:9000/another_app/and_stream</Url>
			//              </Urls>
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

			found_matched_origin = (url_list->size() > 0) ? &origin : nullptr;
		}
	}

	if ((found_matched_host != nullptr) && (found_matched_origin != nullptr))
	{
		if (matched_host != nullptr)
		{
			*matched_host = found_matched_host;
		}

		if (matched_origin != nullptr)
		{
			*matched_origin = found_matched_origin;
		}

		return true;
	}

	return false;
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const ov::String &vhost_name, const info::Application &app_info)
{
	auto vhost = GetVirtualHost(vhost_name);

	if (vhost == nullptr)
	{
		logtw("Host not found for vhost: %s", vhost_name.CStr());
		return Result::Failed;
	}

	auto &app_map = vhost->app_map;
	auto &app_name = app_info.GetName();

	for (auto &app : app_map)
	{
		if (app.second->app_info.GetName() == app_name)
		{
			// The application does exists
			logtd("The application does exists: %s %s", vhost_name.CStr(), app_name.CStr());
			return Result::Exists;
		}
	}

	logti("Trying to create an application: [%s]", app_name.CStr());

	mon::Monitoring::GetInstance()->OnApplicationCreated(app_info);

	// Notify modules of creation events
	std::vector<std::shared_ptr<OrchestratorModuleInterface>> created_list;
	bool succeeded = true;

	auto new_app = std::make_shared<Application>(this, app_info);
	app_map.emplace(app_info.GetId(), new_app);

	for (auto &module : _module_list)
	{
		auto &module_interface = module.module;

		logtd("Notifying %p (%s) for the create event (%s)", module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

		if (module_interface->OnCreateApplication(app_info))
		{
			logtd("The module %p (%s) returns true", module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr());

			created_list.push_back(module_interface);
		}
		else
		{
			logte("The module %p (%s) returns error while creating the application [%s]",
				  module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr(), app_name.CStr());
			succeeded = false;
			break;
		}
	}

	if (_media_router != nullptr)
	{
		_media_router->RegisterObserverApp(app_info, new_app->GetSharedPtrAs<MediaRouteApplicationObserver>());
	}

	if (succeeded)
	{
		return Result::Succeeded;
	}

	logte("Trying to rollback for the application [%s]", app_name.CStr());
	return DeleteApplicationInternal(app_info);
}

Orchestrator::Result Orchestrator::CreateApplicationInternal(const info::VHostAppName &vhost_app_name, info::Application *app_info)
{
	OV_ASSERT2(app_info != nullptr);

	ov::String vhost_name;

	if (ParseVHostAppName(vhost_app_name, &vhost_name, nullptr))
	{
		auto vhost = GetVirtualHost(vhost_name);
		*app_info = info::Application(vhost->host_info, GetNextAppId(), vhost_app_name, true);
		return CreateApplicationInternal(vhost_name, *app_info);
	}

	return Result::Failed;
}

Orchestrator::Result Orchestrator::NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info)
{
	Result result = Result::Succeeded;

	// Notify modules of deletion events
	for (auto module = modules.rbegin(); module != modules.rend(); ++module)
	{
		auto &module_interface = module->module;

		logtd("Notifying %p (%s) for the delete event (%s)", module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

		if (module_interface->OnDeleteApplication(app_info) == false)
		{
			logte("The module %p (%s) returns error while deleting the application %s",
				  module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

			// Ignore this error
			result = Result::Failed;
		}
		else
		{
			logtd("The module %p (%s) returns true", module_interface.get(), GetOrchestratorModuleTypeName(module_interface->GetModuleType()).CStr());
		}
	}

	return result;
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(const ov::String &vhost_name, info::application_id_t app_id)
{
	auto vhost = GetVirtualHost(vhost_name);

	if (vhost == nullptr)
	{
		return Result::Failed;
	}

	auto &app_map = vhost->app_map;
	auto app_item = app_map.find(app_id);
	if (app_item == app_map.end())
	{
		logti("Application %d does not exists", app_id);
		return Result::NotExists;
	}

	auto app = app_item->second;
	auto &app_info = app->app_info;

	logti("Trying to delete an application: [%s] (%u)", app_info.GetName().CStr(), app_info.GetId());

	mon::Monitoring::GetInstance()->OnApplicationDeleted(app_info);

	app_map.erase(app_id);

	if (_media_router != nullptr)
	{
		_media_router->UnregisterObserverApp(app_info, app->GetSharedPtrAs<MediaRouteApplicationObserver>());
	}

	logtd("Notifying modules for the delete event...");
	return NotifyModulesForDeleteEvent(_module_list, app_info);
}

Orchestrator::Result Orchestrator::DeleteApplicationInternal(const info::Application &app_info)
{
	ov::String vhost_name;

	if (ParseVHostAppName(app_info.GetName(), &vhost_name, nullptr) == false)
	{
		return Result::Failed;
	}

	return DeleteApplicationInternal(vhost_name, app_info.GetId());
}

Orchestrator::Result Orchestrator::CreateApplication(const info::Host &host_info, const cfg::Application &app_config)
{
	auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

	auto vhost_name = host_info.GetName();

	info::Application app_info(host_info, GetNextAppId(), ResolveApplicationName(vhost_name, app_config.GetName()), app_config, false);

	return CreateApplicationInternal(vhost_name, app_info);
}

Orchestrator::Result Orchestrator::DeleteApplication(const info::Application &app_info)
{
	auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

	return DeleteApplicationInternal(app_info);
}

Orchestrator::Result Orchestrator::Release()
{
	auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

	// Mark all items as NeedToCheck
	for (auto &vhost_item : _virtual_host_list)
	{
		mon::Monitoring::GetInstance()->OnHostDeleted(vhost_item->host_info);

		auto app_map = vhost_item->app_map;
		for (auto &app_item : app_map)
		{
			auto &app_info = app_item.second->app_info;

			auto result = DeleteApplicationInternal(app_info);
			if (result != Result::Succeeded)
			{
				logte("Could not delete application: %s", app_info.GetName().CStr());
				continue;
			}
		}

		app_map.clear();
	}

	_virtual_host_list.clear();
	_virtual_host_map.clear();

	return Result::Succeeded;
}

const info::Application &Orchestrator::GetApplicationInfoInternal(const info::VHostAppName &vhost_app_name) const
{
	ov::String vhost_name;

	if (ParseVHostAppName(vhost_app_name, &vhost_name, nullptr))
	{
		auto vhost = GetVirtualHost(vhost_name);

		if (vhost != nullptr)
		{
			auto &app_map = vhost->app_map;

			for (auto app_item : app_map)
			{
				auto &app_info = app_item.second->app_info;

				if (app_info.GetName() == vhost_app_name)
				{
					return app_info;
				}
			}
		}
	}

	return info::Application::GetInvalidApplication();
}

const info::Application &Orchestrator::GetApplicationInfoInternal(const ov::String &vhost_name, const ov::String &app_name) const
{
	return GetApplicationInfoInternal(ResolveApplicationName(vhost_name, app_name));
}

const info::Application &Orchestrator::GetApplicationInfoInternal(const ov::String &vhost_name, info::application_id_t app_id) const
{
	auto vhost = GetVirtualHost(vhost_name);

	if (vhost != nullptr)
	{
		auto &app_map = vhost->app_map;
		auto app_item = app_map.find(app_id);

		if (app_item != app_map.end())
		{
			return app_item->second->app_info;
		}
	}

	return info::Application::GetInvalidApplication();
}

const info::Application &Orchestrator::GetApplicationInfoByName(const ov::String &vhost_name, const ov::String &app_name) const
{
	auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

	return GetApplicationInfoInternal(vhost_name, app_name);
}

const info::Application &Orchestrator::GetApplicationInfoByVHostAppName(const info::VHostAppName &vhost_app_name) const
{
	auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

	return GetApplicationInfoInternal(vhost_app_name);
}

bool Orchestrator::RequestPullStream(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &app_name, const ov::String &stream_name, const ov::String &url, off_t offset)
{
	auto parsed_url = ov::Url::Parse(url.CStr());

	if (parsed_url != nullptr)
	{
		// The URL has a scheme
		auto source = parsed_url->Source();

		std::shared_ptr<OrchestratorPullProviderModuleInterface> provider_module;
		auto app_info = info::Application::GetInvalidApplication();
		Result result = Result::Failed;

		{
			auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

			{
				auto scoped_lock_for_module_list = std::scoped_lock(_module_list_mutex);
				provider_module = GetProviderModuleForScheme(parsed_url->Scheme());
			}

			if (provider_module == nullptr)
			{
				logte("Could not find provider for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
				return false;
			}

			// Check if the application does exists
			app_info = GetApplicationInfoInternal(vhost_app_name);

			if (app_info.IsValid())
			{
				result = Result::Exists;
			}
			else
			{
				// Create a new application
				result = CreateApplicationInternal(vhost_app_name, &app_info);

				if (
					// Failed to create the application
					(result == Result::Failed) ||
					// result always must be Result::Succeeded
					(result != Result::Succeeded))
				{
					logte("Could not create an application: %s, reason: %d", vhost_app_name.CStr(), static_cast<int>(result));
					return false;
				}
			}
		}

		logti("Trying to pull stream [%s/%s] from provider using URL: %s",
			  vhost_app_name.CStr(), stream_name.CStr(),
			  GetOrchestratorModuleTypeName(provider_module->GetModuleType()).CStr());

		auto stream = provider_module->PullStream(app_info, stream_name, {source}, offset);

		if (stream != nullptr)
		{
			logti("The stream was pulled successfully: [%s/%s] (%u)",
				  vhost_app_name.CStr(), stream_name.CStr(), stream->GetId());

			return true;
		}

		logte("Could not pull stream [%s/%s] from provider: %s",
			  vhost_app_name.CStr(), stream_name.CStr(),
			  GetOrchestratorModuleTypeName(provider_module->GetModuleType()).CStr());

		// Rollback if needed
		switch (result)
		{
			case Result::Failed:
			case Result::NotExists:
				// This is a bug - Must be handled above
				OV_ASSERT2(false);
				break;

			case Result::Succeeded: {
				auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

				// New application is created. Rollback is required
				DeleteApplicationInternal(app_info);
				break;
			}

			case Result::Exists:
				// Used a previously created application. Do not need to rollback
				break;
		}

		return false;
	}
	else
	{
		// Invalid URL
		logte("Pull stream is requested for invalid URL: %s", url.CStr());
	}

	return false;
}

bool Orchestrator::RequestPullStream(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &app_name, const ov::String &stream_name, off_t offset)
{
	std::shared_ptr<OrchestratorPullProviderModuleInterface> provider_module;
	auto app_info = info::Application::GetInvalidApplication();
	Result result = Result::Failed;

	std::vector<ov::String> url_list;

	Origin *matched_origin = nullptr;
	Host *matched_host = nullptr;

	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		if (GetUrlListForLocationInternal(vhost_app_name, host_name, stream_name, &url_list, &matched_origin, &matched_host) == false)
		{
			logte("Could not find Origin for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		if ((matched_origin == nullptr) || (matched_host == nullptr))
		{
			// Origin/Domain can never be nullptr if origin is found
			OV_ASSERT2((matched_origin != nullptr) && (matched_host != nullptr));

			logte("Could not find URL list for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		{
			auto scoped_lock_for_module_list = std::scoped_lock(_module_list_mutex);
			provider_module = GetProviderModuleForScheme(matched_origin->scheme);
		}

		if (provider_module == nullptr)
		{
			logte("Could not find provider for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		// Check if the application does exists
		app_info = GetApplicationInfoInternal(vhost_app_name);

		if (app_info.IsValid())
		{
			result = Result::Exists;
		}
		else
		{
			// Create a new application
			result = CreateApplicationInternal(vhost_app_name, &app_info);

			if (
				// Failed to create the application
				(result == Result::Failed) ||
				// result always must be Result::Succeeded
				(result != Result::Succeeded))
			{
				logte("Could not create an application: %s, reason: %d", vhost_app_name.CStr(), static_cast<int>(result));
				return false;
			}
		}
	}

	logti("Trying to pull stream [%s/%s] from provider using origin map: %s",
		  vhost_app_name.CStr(), stream_name.CStr(),
		  GetOrchestratorModuleTypeName(provider_module->GetModuleType()).CStr());

	auto stream = provider_module->PullStream(app_info, stream_name, url_list, offset);

	if (stream != nullptr)
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		auto stream_id = stream->GetId();

		auto &origin_stream_map = matched_origin->stream_map;
		auto exists_in_origin = (origin_stream_map.find(stream_id) != origin_stream_map.end());

		auto &host_stream_map = matched_host->stream_map[host_name];
		bool exists_in_domain = (host_stream_map.find(stream_id) != host_stream_map.end());

		if (exists_in_origin == exists_in_domain)
		{
			if (exists_in_origin == false)
			{
				// New stream
				auto orchestrator_stream = std::make_shared<Stream>(app_info, provider_module, stream, ov::String::FormatString("%s/%s", vhost_app_name.CStr(), stream_name.CStr()));

				origin_stream_map[stream_id] = orchestrator_stream;
				host_stream_map[stream_id] = orchestrator_stream;

				logti("The stream was pulled successfully: [%s/%s] (%u)", vhost_app_name.CStr(), stream_name.CStr(), stream_id);
				return true;
			}

			// The stream exists
			logti("The stream was pulled successfully (stream exists): [%s/%s] (%u)", vhost_app_name.CStr(), stream_name.CStr(), stream_id);
			return true;
		}
		else
		{
			logtc("Out of sync: origin: %d, domain: %d (This is a bug)", exists_in_origin, exists_in_domain);
			OV_ASSERT2(false);
		}
	}

	logte("Could not pull stream [%s/%s] from provider: %s",
		  vhost_app_name.CStr(), stream_name.CStr(),
		  GetOrchestratorModuleTypeName(provider_module->GetModuleType()).CStr());

	// Rollback if needed
	switch (result)
	{
		case Result::Failed:
		case Result::NotExists:
			// This is a bug - Must be handled above
			OV_ASSERT2(false);
			break;

		case Result::Succeeded: {
			auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

			// New application is created. Rollback is required
			DeleteApplicationInternal(app_info);
			break;
		}

		case Result::Exists:
			// Used a previously created application. Do not need to rollback
			break;
	}

	return false;
}

bool Orchestrator::OnCreateStream(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
{
	logtd("%s stream is created", info->GetName().CStr());
	return true;
}

bool Orchestrator::OnDeleteStream(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
{
	logtd("%s stream is deleted", info->GetName().CStr());
	return true;
}