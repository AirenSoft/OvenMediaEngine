//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "orchestrator_internal.h"

#include <monitoring/monitoring.h>

#include "orchestrator_private.h"

namespace ocst
{
	bool OrchestratorInternal::ApplyForVirtualHost(const std::shared_ptr<VirtualHost> &virtual_host)
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

				auto result = DeleteApplication(app_info);

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
										logte("Failed to stop stream [%s] in provider: %s", stream->full_name.CStr(), GetModuleTypeName(stream->provider->GetModuleType()).CStr());
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
									logte("Failed to stop stream [%s] in provider: %s", stream->full_name.CStr(), GetModuleTypeName(stream->provider->GetModuleType()).CStr());
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

	ocst::ItemState OrchestratorInternal::ProcessHostList(std::vector<Host> *host_list, const cfg::cmn::Host &host_config) const
	{
		bool is_changed = false;

		// TODO(dimiden): Is there a way to reduce the cost of O(n^2)?
		for (auto &host_name : host_config.GetNameList())
		{
			auto name = host_name.GetName();
			bool found = false;

			for (auto &host : *host_list)
			{
				if (host.state == ItemState::NeedToCheck)
				{
					if (host.name == name)
					{
						host.state = ItemState::NotChanged;
						found = true;
						break;
					}
				}
			}

			if (found == false)
			{
				logtd("      - %s: New", host_name.GetName().CStr());
				// Adding items here causes unnecessary iteration in the for statement above
				// To avoid this, we need to create a separate list for each added item
				host_list->push_back(name);
				is_changed = true;
			}
		}

		if (is_changed == false)
		{
			// There was no new item

			// Check for deleted items
			for (auto &host : *host_list)
			{
				switch (host.state)
				{
					case ItemState::NeedToCheck:
						// This item was deleted because it was never processed in the above process
						logtd("      - %s: Deleted", host.name.CStr());
						host.state = ItemState::Delete;
						is_changed = true;
						break;

					case ItemState::NotChanged:
						// Nothing to do
						logtd("      - %s: Not changed", host.name.CStr());
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

	ocst::ItemState OrchestratorInternal::ProcessOriginList(std::vector<Origin> *origin_list, const cfg::vhost::orgn::Origins &origins_config) const
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
								[&origin](const cfg::cmn::Url &url1, const cfg::cmn::Url &url2) -> bool {
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

	info::application_id_t OrchestratorInternal::GetNextAppId()
	{
		return _last_application_id++;
	}

	std::shared_ptr<pvd::Provider> OrchestratorInternal::GetProviderForScheme(const ov::String &scheme)
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
		std::shared_ptr<PullProviderModuleInterface> module = nullptr;

		for (auto info = _module_list.begin(); info != _module_list.end(); ++info)
		{
			if (info->type == ModuleType::PullProvider)
			{
				auto module = std::dynamic_pointer_cast<PullProviderModuleInterface>(info->module);
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

	std::shared_ptr<PullProviderModuleInterface> OrchestratorInternal::GetProviderModuleForScheme(const ov::String &scheme)
	{
		auto provider = GetProviderForScheme(scheme);
		auto provider_module = std::dynamic_pointer_cast<PullProviderModuleInterface>(provider);

		OV_ASSERT((provider == nullptr) || (provider_module != nullptr),
				  "Provider (%d) shouldmust inherit from ProviderModuleInterface",
				  provider->GetProviderType());

		return provider_module;
	}

	std::shared_ptr<pvd::Provider> OrchestratorInternal::GetProviderForUrl(const ov::String &url)
	{
		// Find a provider type using the scheme
		auto parsed_url = ov::Url::Parse(url);

		if (url == nullptr)
		{
			logtw("Could not parse URL: %s", url.CStr());
			return nullptr;
		}

		logtd("Obtaining ProviderType for URL %s...", url.CStr());

		return GetProviderForScheme(parsed_url->Scheme());
	}

	info::VHostAppName OrchestratorInternal::ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name) const
	{
		// Replace all # to _
		return info::VHostAppName(vhost_name, app_name);
	}

	std::shared_ptr<ocst::VirtualHost> OrchestratorInternal::GetVirtualHost(const ov::String &vhost_name)
	{
		auto vhost_item = _virtual_host_map.find(vhost_name);

		if (vhost_item == _virtual_host_map.end())
		{
			return nullptr;
		}

		return vhost_item->second;
	}

	std::shared_ptr<const ocst::VirtualHost> OrchestratorInternal::GetVirtualHost(const ov::String &vhost_name) const
	{
		auto vhost_item = _virtual_host_map.find(vhost_name);

		if (vhost_item == _virtual_host_map.end())
		{
			return nullptr;
		}

		return vhost_item->second;
	}

	std::shared_ptr<ocst::VirtualHost> OrchestratorInternal::GetVirtualHost(const info::VHostAppName &vhost_app_name)
	{
		if (vhost_app_name.IsValid())
		{
			return GetVirtualHost(vhost_app_name.GetVHostName());
		}

		// vhost_app_name must be valid
		OV_ASSERT2(false);
		return nullptr;
	}

	std::shared_ptr<const ocst::VirtualHost> OrchestratorInternal::GetVirtualHost(const info::VHostAppName &vhost_app_name) const
	{
		if (vhost_app_name.IsValid())
		{
			return GetVirtualHost(vhost_app_name.GetVHostName());
		}

		// vhost_app_name must be valid
		OV_ASSERT2(false);
		return nullptr;
	}

	bool OrchestratorInternal::GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list, Origin **matched_origin, Host **matched_host)
	{
		auto vhost = GetVirtualHost(vhost_app_name);

		if (vhost == nullptr)
		{
			logte("Could not find VirtualHost for the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		Host *found_matched_host = nullptr;
		Origin *found_matched_origin = nullptr;

		auto &host_list = vhost->host_list;
		auto &origin_list = vhost->origin_list;

		ov::String location = ov::String::FormatString("/%s/%s", vhost_app_name.GetAppName().CStr(), stream_name.CStr());

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

				logtd("Found: location: %s (app: %s, stream: %s), remaining_part: %s", origin.location.CStr(), vhost_app_name.GetAppName().CStr(), stream_name.CStr(), remaining_part.CStr());

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

					// Exclude query string from url
					auto index = url.IndexOf('?');
					auto url_part = url.Substring(0, index);
					auto another_part = url.Substring(index + 1);

					// Append remaining_part
					url_part.Append(remaining_part);

					if(index >= 0)
					{
						url_part.Append('?');
						url_part.Append(another_part);
					}

					url_list->push_back(url_part);
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

	ocst::Result OrchestratorInternal::CreateApplication(const ov::String &vhost_name, const info::Application &app_info)
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
		std::vector<std::shared_ptr<ModuleInterface>> created_list;
		bool succeeded = true;

		auto new_app = std::make_shared<Application>(this, app_info);
		app_map.emplace(app_info.GetId(), new_app);

		for (auto &module : _module_list)
		{
			auto &module_interface = module.module;

			logtd("Notifying %p (%s) for the create event (%s)", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

			if (module_interface->OnCreateApplication(app_info))
			{
				logtd("The module %p (%s) returns true", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr());

				created_list.push_back(module_interface);
			}
			else
			{
				logte("The module %p (%s) returns error while creating the application [%s]",
					  module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_name.CStr());
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
		return DeleteApplication(app_info);
	}

	ocst::Result OrchestratorInternal::CreateApplication(const info::VHostAppName &vhost_app_name, info::Application *app_info)
	{
		OV_ASSERT2(app_info != nullptr);

		if (vhost_app_name.IsValid())
		{
			auto &vhost_name = vhost_app_name.GetVHostName();
			auto vhost = GetVirtualHost(vhost_name);

			*app_info = info::Application(vhost->host_info, GetNextAppId(), vhost_app_name, true);

			return CreateApplication(vhost_name, *app_info);
		}

		return Result::Failed;
	}

	ocst::Result OrchestratorInternal::NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info)
	{
		Result result = Result::Succeeded;

		// Notify modules of deletion events
		for (auto module = modules.rbegin(); module != modules.rend(); ++module)
		{
			auto &module_interface = module->module;

			logtd("Notifying %p (%s) for the delete event (%s)", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

			if (module_interface->OnDeleteApplication(app_info) == false)
			{
				logte("The module %p (%s) returns error while deleting the application %s",
					  module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

				// Ignore this error
				result = Result::Failed;
			}
			else
			{
				logtd("The module %p (%s) returns true", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr());
			}
		}

		return result;
	}

	ocst::Result OrchestratorInternal::DeleteApplication(const ov::String &vhost_name, info::application_id_t app_id)
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

	ocst::Result OrchestratorInternal::DeleteApplication(const info::Application &app_info)
	{
		auto &vhost_app_name = app_info.GetName();

		if(vhost_app_name.IsValid() == false)
		{
			return Result::Failed;
		}

		return DeleteApplication(vhost_app_name.GetVHostName(), app_info.GetId());
	}

	const info::Application &OrchestratorInternal::GetApplicationInfo(const info::VHostAppName &vhost_app_name) const
	{
		if(vhost_app_name.IsValid())
		{
			auto &vhost_name = vhost_app_name.GetVHostName();
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

	const info::Application &OrchestratorInternal::GetApplicationInfo(const ov::String &vhost_name, const ov::String &app_name) const
	{
		return GetApplicationInfo(ResolveApplicationName(vhost_name, app_name));
	}

	const info::Application &OrchestratorInternal::GetApplicationInfo(const ov::String &vhost_name, info::application_id_t app_id) const
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
}  // namespace ocst