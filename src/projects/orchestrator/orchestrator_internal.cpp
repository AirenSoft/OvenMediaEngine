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
#include <base/provider/pull_provider/stream.h>
#include "orchestrator_private.h"

namespace ocst
{
	bool OrchestratorInternal::UpdateVirtualHost(const std::shared_ptr<VirtualHost> &virtual_host)
	{
		auto succeeded = true;

		logtd("Trying to apply new configuration of VirtualHost: %s...", virtual_host->name.CStr());

		if (virtual_host->state == ItemState::Delete)
		{
			logtd("VirtualHost is deleted");

			// Delete all apps that were created by this VirtualHost
			auto app_map = virtual_host->app_map;
			for (auto app_item : app_map)
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
			bool found = false;

			for (auto &host : *host_list)
			{
				if (host.state == ItemState::NeedToCheck)
				{
					if (host.name == host_name)
					{
						host.state = ItemState::NotChanged;
						found = true;
						break;
					}
				}
			}

			if (found == false)
			{
				logtd("      - %s: New", host_name.CStr());
				// Adding items here causes unnecessary iteration in the for statement above
				// To avoid this, we need to create a separate list for each added item
				host_list->push_back(host_name);
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
								[&origin](const ov::String &url1, const ov::String &url2) -> bool {
									bool result = url1 == url2;

									if (result == false)
									{
										logtd("      - %s: Changed (URL does not the same: %s != %s)", origin.location.CStr(), url1.CStr(), url2.CStr());
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
		else if (lower_scheme == "file")
		{
			type = ProviderType::File;
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
				  "Provider (%d) must inherit from ProviderModuleInterface",
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

	Result OrchestratorInternal::CreateVirtualHost(const info::Host &vhost_info)
	{
		if(GetVirtualHost(vhost_info.GetName()) != nullptr)
		{
			// Duplicated VirtualHostName
			return Result::Exists;
		}

		auto vhost = std::make_shared<VirtualHost>(vhost_info);
		vhost->name = vhost_info.GetName();

		for (auto &domain_name : vhost_info.GetHost().GetNameList())
		{
			vhost->host_list.emplace_back(domain_name);
		}

		for (auto &origin_config : vhost_info.GetOriginList())
		{
			vhost->origin_list.emplace_back(origin_config);
		}

		bool enabled = false;
		auto store = vhost_info.GetOriginMapStore(&enabled);
		if (enabled == true)
		{
			// Make ovt base url
			auto ovt_port = cfg::ConfigManager::GetInstance()->GetServer()->GetBind().GetPublishers().GetOvt().GetPort();
			vhost->origin_map_client = std::make_shared<OriginMapClient>(store.GetRedisServer().GetHost(), store.GetRedisServer().GetAuth());
			vhost->is_origin_map_store_enabled = true;

			if (store.GetOriginHostName().IsEmpty() == false)
			{
				vhost->origin_base_url = ov::String::FormatString("ovt://%s:%d", store.GetOriginHostName().CStr(), ovt_port.GetPort());
			}
			else
			{
				logti("OriginMapStore::OriginHostName is not specified. This OriginMapStore can work only as a edge.");
			}
		}

		bool is_cors_parsed;
		auto cross_domains = vhost_info.GetCrossDomainList(&is_cors_parsed);

		if (is_cors_parsed)
		{
			// VHOST has no VHostAppName so we use InvalidVHostAppName
			// Each vhost has its own cors manager so there is no problem to use InvalidVHostAppName
			vhost->default_cors_manager.SetCrossDomains(info::VHostAppName::InvalidVHostAppName(), cross_domains);
		}

		_virtual_host_map[vhost_info.GetName()] = vhost;
		_virtual_host_list.push_back(vhost);

		// Notification 
		for (auto &module : _module_list)
		{
			auto &module_interface = module.module;

			logtd("Notifying %p (%s) for the create event (%s)", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());

			if (module_interface->OnCreateHost(vhost_info))
			{
				logtd("The module %p (%s) returns true", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr());
			}
			else
			{
				logte("The module %p (%s) returns error while creating the vhost [%s]",
					  module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());
			}
		}

		mon::Monitoring::GetInstance()->OnHostCreated(vhost_info);

		return Result::Succeeded;
	}

	Result OrchestratorInternal::DeleteVirtualHost(const info::Host &vhost_info)
	{
		auto i = _virtual_host_list.begin();
		while(i != _virtual_host_list.end())
		{
			auto vhost_item = *i;
			if(vhost_item->name == vhost_info.GetName())
			{
				_virtual_host_list.erase(i);
				_virtual_host_map.erase(vhost_item->name);


				// Notification
				for (auto &module : _module_list)
				{
					auto &module_interface = module.module;

					logtd("Notifying %p (%s) for the create event (%s)", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());

					if (module_interface->OnDeleteHost(vhost_info))
					{
						logtd("The module %p (%s) returns true", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr());
					}
					else
					{
						logte("The module %p (%s) returns error while deleting the vhost [%s]",
							module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());
					}
				}
				
				mon::Monitoring::GetInstance()->OnHostDeleted(vhost_info);
				return Result::Succeeded;
			}

			i++;
		}

		return Result::NotExists;
	}

	Result OrchestratorInternal::ReloadAllCertificates()
	{
		Result total_result = Result::Succeeded;

		for (auto &vhost : _virtual_host_list)
		{
			auto result = ReloadCertificate(vhost);
			if (result != Result::Succeeded)
			{
				total_result = Result::Failed;
			}
		}

		return total_result;
	}

	
	Result OrchestratorInternal::ReloadCertificate(const ov::String &vhost_name)
	{
		auto vhost = GetVirtualHost(vhost_name);
		if (vhost == nullptr)
		{
			return Result::NotExists;
		}

		return ReloadCertificate(vhost);
	}

	Result OrchestratorInternal::ReloadCertificate(const std::shared_ptr<VirtualHost> &vhost)
	{
		auto &vhost_info = vhost->host_info;

		if (vhost_info.LoadCertificate() == false)
		{
			logte("Could not load certificate for vhost [%s]", vhost_info.GetName().CStr());
			return Result::Failed;
		}

		// Notification
		for (auto &module : _module_list)
		{
			auto &module_interface = module.module;

			logtd("Notifying %p (%s) for the create event (%s)", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());

			if (module_interface->OnUpdateCertificate(vhost_info))
			{
				logtd("The module %p (%s) returns true", module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr());
			}
			else
			{
				logte("The module %p (%s) returns error while updating certificate of the vhost [%s]",
					module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), vhost_info.GetName().CStr());
			}
		}

		return Result::Succeeded;
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
		logtd("Trying to find the item from origin_list that match location: %s", location.CStr());		

		for (auto &origin : origin_list)
		{
			// TODO(dimiden): Replace with the regex
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


				if(remaining_part.GetLength() > 0 && origin.strict_location == true) {
					logtw("This origin does not support match sub location. matched location: %s, requested location: %s", origin.location.CStr(), location.CStr());
					continue;
				}

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

					// Exception to avoid url parsing errors. Patterns like "file:///path/filename.mp4" cannot be parsed by ov:Url.
					// So, added a fake host string. Finally, need to update the regex pattern of ov::URL.
					// TODO: Improve ov::URL class 
					// TODO: Replace with the regex
					if(url.HasPrefix(":///")) {
						url = url.Replace(":///", "://fakeip/");
					}

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

	Result OrchestratorInternal::CreateApplicationTemplate(const info::Host &host_info, const cfg::vhost::app::Application &app_config)
	{
		if (app_config.GetName() != "*")
		{
			logtw("Application template name must be \"*\" : %s", app_config.GetName().CStr());
			return Result::Failed;
		}

		auto vhost = GetVirtualHost(host_info.GetName());
		if (vhost == nullptr)
		{
			logtw("Host not found for vhost: %s", host_info.GetName().CStr());
			return Result::Failed;
		}

		vhost->app_cfg_template = app_config;

		return Result::Succeeded;
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
			}
			else
			{
				logte("The module %p (%s) returns error while creating the application [%s]",
					  module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_name.CStr());
				succeeded = false;
				break;
			}
		}

		// TODO: Need to be refactored
		// Since Orchestrator registers itself as MediaRouter observer last, OnStreamCreated and OnStreamDeleted events are received last.
		// Orchestrator::OnStreamDeleted and application deletion can proceed simultaneously if the orchestrator does not receive the event at the 
		// very end, so if stream deletion is still in progress in another module, this may cause a conflict.
		// Therefore, we need a guaranteed way Orchestrator to receive the event last, 
		// not the way the orchestrator registers itself with the MediaRouter last and receives the event last. 
		// (Now, it's working because MediaRouter registers an observer using push_back to the vector.)
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
				logte("The module %p (%s) returns error while deleting the application [%s]",
					  module_interface.get(), GetModuleTypeName(module_interface->GetModuleType()).CStr(), app_info.GetName().CStr());

				// Ignore this error - some providers may not have generated the app
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

		if (vhost_app_name.IsValid() == false)
		{
			return Result::Failed;
		}

		return DeleteApplication(vhost_app_name.GetVHostName(), app_info.GetId());
	}

	std::shared_ptr<Application> OrchestratorInternal::GetApplication(const info::VHostAppName &vhost_app_name) const
	{
		if (vhost_app_name.IsValid())
		{
			auto &vhost_name = vhost_app_name.GetVHostName();
			auto vhost = GetVirtualHost(vhost_name);

			if (vhost != nullptr)
			{
				auto &app_map = vhost->app_map;

				for (auto app_item : app_map)
				{
					auto &app = app_item.second;
					auto &app_info = app->app_info;

					if (app_info.GetName() == vhost_app_name)
					{
						return app;
					}
				}
			}
		}

		return nullptr;
	}

	const info::Application &OrchestratorInternal::GetApplicationInfo(const info::VHostAppName &vhost_app_name) const
	{
		auto app = GetApplication(vhost_app_name);
		if (app == nullptr)
		{
			return info::Application::GetInvalidApplication();
		}

		return app->app_info;
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

	std::shared_ptr<pvd::Stream> OrchestratorInternal::GetProviderStream(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		auto app = GetApplication(vhost_app_name);

		if (app != nullptr)
		{
			return app->GetProviderStream(stream_name);
		}

		return nullptr;
	}
}  // namespace ocst