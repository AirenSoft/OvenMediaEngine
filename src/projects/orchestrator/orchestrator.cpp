//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "orchestrator.h"

#include <base/mediarouter/mediarouter_interface.h>
#include <base/provider/stream.h>
#include <base/provider/pull_provider/stream_props.h>
#include <mediarouter/mediarouter.h>
#include <monitoring/monitoring.h>

#include "orchestrator_private.h"

namespace ocst
{
	bool Orchestrator::StartServer(const std::shared_ptr<const cfg::Server> &server_config)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		_server_config = server_config;

		mon::Monitoring::GetInstance()->OnServerStarted(server_config);

		auto &vhost_conf_list = _server_config->GetVirtualHostList();

		return CreateVirtualHosts(vhost_conf_list);
	}

	ocst::Result Orchestrator::Release()
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

				auto result = OrchestratorInternal::DeleteApplication(app_info);
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

		mon::Monitoring::GetInstance()->Release();

		return Result::Succeeded;
	}

	bool Orchestrator::CreateVirtualHosts(const std::vector<cfg::vhost::VirtualHost> &vhost_conf_list)
	{
		for (const auto &vhost_conf : vhost_conf_list)
		{
			// Create VirtualHost in Orchestrator
			if (CreateVirtualHost(vhost_conf) != Result::Succeeded)
			{
				logte("Could not create VirtualHost(%s)", vhost_conf.GetName().CStr());
				return false;
			}
		}

		return true;
	}


	Result Orchestrator::CreateVirtualHost(const cfg::vhost::VirtualHost &vhost_cfg)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		info::Host vhost_info(_server_config->GetName(), _server_config->GetID(), vhost_cfg);

		auto result = OrchestratorInternal::CreateVirtualHost(vhost_info);
		switch (result)
		{
			case ocst::Result::Failed:
				logtc("Failed to create a virtual host: %s", vhost_cfg.GetName().CStr());
				return result;

			case ocst::Result::Succeeded:
				break;

			case ocst::Result::Exists:
				logtc("Duplicate virtual host [%s] found. Please check the settings.", vhost_cfg.GetName().CStr());
				return result;

			case ocst::Result::NotExists:
				// This should never happen
				OV_ASSERT2(false);
				logtc("Internal error occurred (THIS IS A BUG)");
				return result;
		}

		// Create Applications
		for (const auto &app_cfg : vhost_info.GetApplicationList())
		{
			if(CreateApplication(vhost_info, app_cfg) != Result::Succeeded)
			{
				// Rollback
				DeleteVirtualHost(vhost_info);
				return Result::Failed;
			}
		}

		return Result::Succeeded;
	}

	Result Orchestrator::DeleteVirtualHost(const info::Host &vhost_info)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		// Delete Applications
		auto vhost = GetVirtualHost(vhost_info.GetName());
		// Copy app list so that 
		auto app_map = vhost->app_map;
		for(const auto &item : app_map)
		{
			auto app = item.second;
			if(DeleteApplication(app->app_info) != Result::Succeeded)
			{
				logtc("Failed to delete an application (%s) in virtual host (%s)", app->app_info.GetName().CStr(), vhost_info.GetName().CStr());
				return Result::Failed;
			}
		}

		auto result = OrchestratorInternal::DeleteVirtualHost(vhost_info);
		switch (result)
		{
			case ocst::Result::Failed:
				logtc("Failed to delete a virtual host: %s", vhost_info.GetName().CStr());
				return result;

			case ocst::Result::Succeeded:
				break;

			case ocst::Result::Exists:
				// This should never happen
				OV_ASSERT2(false);
				logtc("Duplicate virtual host [%s] found. Please check the settings.", vhost_info.GetName().CStr());
				return result;

			case ocst::Result::NotExists:
				logtc("Unable to delete vhost (does not exist): %s", vhost_info.GetName().CStr());
				return result;
		}

		return result;
	}

	std::optional<info::Host> Orchestrator::GetHostInfo(ov::String vhost_name)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		auto vhost = OrchestratorInternal::GetVirtualHost(vhost_name);

		if(vhost != nullptr)
		{
			return vhost->host_info;
		}

		return std::nullopt;
	}

	ocst::Result Orchestrator::CreateApplication(const info::Host &host_info, const cfg::vhost::app::Application &app_config)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		auto vhost_name = host_info.GetName();

		info::Application app_info(host_info, GetNextAppId(), ResolveApplicationName(vhost_name, app_config.GetName()), app_config, false);

		auto result = OrchestratorInternal::CreateApplication(vhost_name, app_info);
		switch (result)
		{
			case ocst::Result::Failed:
				logtc("Failed to create an application: %s/%s", vhost_name.CStr(), app_config.GetName().CStr());
				break;

			case ocst::Result::Succeeded:
				break;

			case ocst::Result::Exists:
				logtc("Duplicate application [%s/%s] found. Please check the settings.", vhost_name.CStr(), app_config.GetName().CStr());
				break;

			case ocst::Result::NotExists:
				// This should never happen
				OV_ASSERT2(false);
				logtc("Internal error occurred (THIS IS A BUG)");
				break;
		}

		return result;
	}

	ocst::Result Orchestrator::DeleteApplication(const info::Application &app_info)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		auto result = OrchestratorInternal::DeleteApplication(app_info);
		switch (result)
		{
			case ocst::Result::Failed:
				logtc("Failed to delete an application: %s", app_info.GetName().CStr());
				return result;

			case ocst::Result::Succeeded:
				break;

			case ocst::Result::Exists:
				// This should never happen
				OV_ASSERT2(false);
				logtc("Duplicate application [%s] found. Please check the settings.", app_info.GetName().CStr());
				return result;

			case ocst::Result::NotExists:
				logtc("Unable to delete application (does not exist): %s", app_info.GetName().CStr());
				return result;
		}

		return result;
	}

	const info::Application &Orchestrator::GetApplicationInfo(const ov::String &vhost_name, const ov::String &app_name) const
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		return OrchestratorInternal::GetApplicationInfo(vhost_name, app_name);
	}

	const info::Application &Orchestrator::GetApplicationInfo(const info::VHostAppName &vhost_app_name) const
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		return OrchestratorInternal::GetApplicationInfo(vhost_app_name);
	}


	bool Orchestrator::UpdateVirtualHosts(const std::vector<info::Host> &host_list)
	{
		bool result = true;
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		// Mark all items as NeedToCheck
		for (auto &vhost_item : _virtual_host_map)
		{
			if (vhost_item.second->MarkAllAs(ItemState::NeedToCheck, 2, ItemState::New, ItemState::Applied) == false)
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

			// New VHost
			if (previous_vhost_item == _virtual_host_map.end())
			{
				CreateVirtualHost(host_info);
				continue;
			}

			// If the vhost is exist(matching by name) check if there is updated info (Host, Origin)
			auto &vhost = previous_vhost_item->second;

			logtd("    - Processing for hosts");
			auto new_state_for_host = ProcessHostList(&(vhost->host_list), host_info.GetHost());
			logtd("    - Processing for origins");
			auto new_state_for_origin = ProcessOriginList(&(vhost->origin_list), host_info.GetOrigins());

			if ((new_state_for_host == ItemState::NotChanged) && (new_state_for_origin == ItemState::NotChanged))
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
					UpdateVirtualHost(vhost);

					break;

				case ItemState::NeedToCheck:
					// This item was deleted because it was never processed in the above process
					logtd("  - %s: Deleted", vhost->name.CStr());

					vhost_item = _virtual_host_list.erase(vhost_item);
					_virtual_host_map.erase(vhost->name);

					vhost->MarkAllAs(ItemState::Delete);
					UpdateVirtualHost(vhost);

					break;

				case ItemState::NotChanged:
				case ItemState::New:
					// Nothing to do
					vhost->MarkAllAs(ItemState::Applied);
					++vhost_item;
					break;

				case ItemState::Changed:
					++vhost_item;

					if (UpdateVirtualHost(vhost))
					{
						vhost->MarkAllAs(ItemState::Applied);
					}

					break;
			}
		}

		logtd("All items are applied");

		return result;
	}

	std::vector<std::shared_ptr<ocst::VirtualHost>> Orchestrator::GetVirtualHostList()
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);
		return _virtual_host_list;
	}

	bool Orchestrator::RegisterModule(const std::shared_ptr<ModuleInterface> &module)
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
					logtw("%s module (%p) is already registered", GetModuleTypeName(type).CStr(), module.get());
				}
				else
				{
					logtw("The module type was %s (%p), but now %s", GetModuleTypeName(info.type).CStr(), module.get(), GetModuleTypeName(type).CStr());
				}

				OV_ASSERT2(false);
				return false;
			}
		}

		_module_list.emplace_back(type, module);

		if (module->GetModuleType() == ModuleType::MediaRouter)
		{
			auto media_router = std::dynamic_pointer_cast<MediaRouter>(module);

			OV_ASSERT2(media_router != nullptr);

			_media_router = media_router;
		}

		logtd("%s module (%p) is registered", GetModuleTypeName(type).CStr(), module.get());

		return true;
	}

	bool Orchestrator::UnregisterModule(const std::shared_ptr<ModuleInterface> &module)
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
				logtd("%s module (%p) is unregistered", GetModuleTypeName(info->type).CStr(), module.get());
				return true;
			}
		}

		logtw("%s module (%p) not found", GetModuleTypeName(module->GetModuleType()).CStr(), module.get());
		OV_ASSERT2(false);

		return false;
	}

	ov::String Orchestrator::GetVhostNameFromDomain(const ov::String &domain_name) const
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

	info::VHostAppName Orchestrator::ResolveApplicationNameFromDomain(const ov::String &domain_name, const ov::String &app_name) const
	{
		auto vhost_name = GetVhostNameFromDomain(domain_name);

		if (vhost_name.IsEmpty())
		{
			logtw("Could not find VirtualHost for domain: %s", domain_name.CStr());
		}

		auto resolved = ResolveApplicationName(vhost_name, app_name);

		logtd("Resolved application name: %s (from domain: %s, app: %s)", resolved.CStr(), domain_name.CStr(), app_name.CStr());

		return resolved;
	}

	bool Orchestrator::GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list)
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		return OrchestratorInternal::GetUrlListForLocation(vhost_app_name, host_name, stream_name, url_list, nullptr, nullptr);
	}

	bool Orchestrator::RequestPullStream(
			const std::shared_ptr<const ov::Url> &request_from,
			const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
			const std::vector<ov::String> &url_list, off_t offset, const std::shared_ptr<pvd::PullStreamProperties> &properties)
	{
		if (url_list.empty() == true)
		{
			logtw("RequestPullStream must have at least one URL");
			return false;
		}

		auto url = url_list[0];
		auto parsed_url = ov::Url::Parse(url);

		if (parsed_url != nullptr)
		{
			std::shared_ptr<PullProviderModuleInterface> provider_module;
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
				app_info = OrchestratorInternal::GetApplicationInfo(vhost_app_name);

				if (app_info.IsValid())
				{
					result = Result::Exists;
				}
				else
				{
					// Create a new application
					result = OrchestratorInternal::CreateApplication(vhost_app_name, &app_info);

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
				  GetModuleTypeName(provider_module->GetModuleType()).CStr());

			
			auto stream = provider_module->PullStream(request_from, app_info, stream_name, url_list, offset, properties);

			if (stream != nullptr)
			{
				StorePullStream(stream);
				logti("The stream was pulled successfully: [%s/%s] (%u)",
					  vhost_app_name.CStr(), stream_name.CStr(), stream->GetId());

				return true;
			}

			logte("Could not pull stream [%s/%s] from provider: %s",
				  vhost_app_name.CStr(), stream_name.CStr(),
				  GetModuleTypeName(provider_module->GetModuleType()).CStr());

			// Rollback if needed
			switch (result)
			{
				case Result::Failed:
				case Result::NotExists:
					// This is a bug - Must be handled above
					logtc("Result is not expected: %d (This is a bug)", result);
					OV_ASSERT2(false);
					break;

				case Result::Succeeded: {
					auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

					// New application is created. Rollback is required
					OrchestratorInternal::DeleteApplication(app_info);
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

	bool Orchestrator::RequestPullStream(
		const std::shared_ptr<const ov::Url> &request_from,
		const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
		const ov::String &url, off_t offset)
	{
		auto properties = std::make_shared<pvd::PullStreamProperties>();
		auto url_item = ov::Url::Parse(url);
		if (url_item->Scheme().UpperCaseString() == "OVT")
		{
			properties->SetRelay(true);
		}

		return RequestPullStream(request_from, vhost_app_name, stream_name, {url}, offset, properties);
	}

	// Pull a stream using Origin map
	bool Orchestrator::RequestPullStream(
		const std::shared_ptr<const ov::Url> &request_from,
		const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
		off_t offset)
	{
		std::shared_ptr<PullProviderModuleInterface> provider_module;
		auto app_info = info::Application::GetInvalidApplication();
		Result result = Result::Failed;

		std::vector<ov::String> url_list;

		Origin *matched_origin = nullptr;
		Host *matched_host = nullptr;

		auto &host_name = request_from->Host();

		{
			auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

			std::vector<ov::String> url_list_in_map;

			if (OrchestratorInternal::GetUrlListForLocation(vhost_app_name, host_name, stream_name, &url_list_in_map, &matched_origin, &matched_host) == false)
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
			app_info = OrchestratorInternal::GetApplicationInfo(vhost_app_name);

			if (app_info.IsValid())
			{
				result = Result::Exists;
			}
			else
			{
				// Create a new application
				result = OrchestratorInternal::CreateApplication(vhost_app_name, &app_info);

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

			if (matched_origin->forward_query_params && request_from->HasQueryString())
			{
				// Combine query string with the URL
				for (auto url : url_list_in_map)
				{
					auto parsed_url = ov::Url::Parse(url);

					url.Append(parsed_url->HasQueryString() ? '&' : '?');
					url.Append(request_from->Query());

					url_list.push_back(url);
				}
			}
			else
			{
				url_list = std::move(url_list_in_map);
			}
		}

		logti("Trying to pull stream [%s/%s] from provider using origin map: %s",
			  vhost_app_name.CStr(), stream_name.CStr(),
			  GetModuleTypeName(provider_module->GetModuleType()).CStr());

		// Use Matched Origin information as an properties in Pull Stream.
		auto stream = provider_module->PullStream(request_from, app_info, stream_name, url_list, offset, 
			std::make_shared<pvd::PullStreamProperties>(matched_origin->_persistent, matched_origin->_failback, matched_origin->_relay));

		if (stream != nullptr)
		{
			StorePullStream(stream);
			return true;

			////////////////////////////////
			// Comment(Getroot): 2022-10-05
			// The code commented below exists for UpdateVirtualHosts. 
			// However, since the UpdateVirtualHosts function is currently deprecated, comment out the code below. 
			// If the UpdateVirtualHosts function is developed again in the future, consider the code below again.
			////////////////////////////////

			// auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

			// auto stream_id = stream->GetId();

			// auto &origin_stream_map = matched_origin->stream_map;
			// auto exists_in_origin = (origin_stream_map.find(stream_id) != origin_stream_map.end());

			// auto key_pair = std::pair(host_name, vhost_app_name.GetAppName());

			// auto &host_stream_map = matched_host->stream_map[key_pair];
			// bool exists_in_domain = (host_stream_map.find(stream_id) != host_stream_map.end());

			// if (exists_in_origin == exists_in_domain)
			// {
			// 	if (exists_in_origin == false)
			// 	{
			// 		// New stream
			// 		auto orchestrator_stream = std::make_shared<Stream>(app_info, provider_module, stream, ov::String::FormatString("%s/%s", vhost_app_name.CStr(), stream_name.CStr()));

			// 		origin_stream_map[stream_id] = orchestrator_stream;
			// 		host_stream_map[stream_id] = orchestrator_stream;

			// 		logti("The stream was pulled successfully: [%s/%s] (%u)", vhost_app_name.CStr(), stream_name.CStr(), stream_id);
			// 		return true;
			// 	}

			// 	// The stream exists
			// 	logti("The stream was pulled successfully (stream exists): [%s/%s] (%u)", vhost_app_name.CStr(), stream_name.CStr(), stream_id);
			// 	return true;
			// }
			// else
			// {
			// 	logtc("Out of sync: origin: %d, domain: %d (This is a bug)", exists_in_origin, exists_in_domain);
			// 	// TODO
			// 	// OV_ASSERT2(false);
			// }
		}

		logte("Could not pull stream [%s/%s] from provider: %s",
			  vhost_app_name.CStr(), stream_name.CStr(),
			  GetModuleTypeName(provider_module->GetModuleType()).CStr());

		// Rollback if needed
		switch (result)
		{
			case Result::Failed:
			case Result::NotExists:
				// This is a bug - Must be handled above
				logtc("Result is not expected: %d (This is a bug)", result);
				OV_ASSERT2(false);
				break;

			case Result::Succeeded: {
				auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

				// New application is created. Rollback is required
				OrchestratorInternal::DeleteApplication(app_info);
				break;
			}

			case Result::Exists:
				// Used a previously created application. Do not need to rollback
				break;
		}

		return false;
	}

	/// Delete PullStream
	bool Orchestrator::RequestReleasePulledStream(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		auto pull_stream = GetPullStream(vhost_app_name, stream_name);
		if (pull_stream == nullptr)
		{
			return false;
		}

		pull_stream->Terminate();

		return true;
	}

	bool Orchestrator::OnStreamCreated(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s/%s stream of %s is created", app_info.GetName().CStr(), info->GetName().CStr(), info->IsInputStream()?"inbound":"outbound");

		return true;
	}

	bool Orchestrator::OnStreamDeleted(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s/%s stream of %s is deleted", app_info.GetName().CStr(), info->GetName().CStr(), info->IsInputStream()?"inbound":"outbound");

		RemovePullStream(info->GetId());

		return true;
	}

	bool Orchestrator::OnStreamPrepared(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s/%s stream of %s is parsed", app_info.GetName().CStr(), info->GetName().CStr(), info->IsInputStream()?"inbound":"outbound");

		CreatePersistentStreamIfNeed(app_info, info);

		return true;
	}

	bool Orchestrator::OnStreamUpdated(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s/%s stream of %s is updated", app_info.GetName().CStr(), info->GetName().CStr(), info->IsInputStream()?"inbound":"outbound");
		return true;
	}

	std::shared_ptr<pvd::Provider> Orchestrator::GetProviderFromType(const ProviderType type)
	{
		// Find the provider
		for (auto info = _module_list.begin(); info != _module_list.end(); ++info)
		{
			if (info->type == ModuleType::PushProvider || info->type == ModuleType::PullProvider)
			{
				auto provider = std::dynamic_pointer_cast<pvd::Provider>(info->module);

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

		logtw("Provider (%d) is not found from type");

		return nullptr;
	}

	std::shared_ptr<pub::Publisher> Orchestrator::GetPublisherFromType(const PublisherType type)
	{
		// Find the publisehr
		for (auto info = _module_list.begin(); info != _module_list.end(); ++info)
		{
			if (info->type == ModuleType::Publisher)
			{
				auto publisher = std::dynamic_pointer_cast<pub::Publisher>(info->module);

				if (publisher == nullptr)
				{
					OV_ASSERT(publisher != nullptr, "Provider must inherit from pub::Publisehr");
					continue;
				}

				if (publisher->GetPublisherType() == type)
				{
					return publisher;
				}
			}
		}

		logtw("Publisher (%d) is not found from type");

		return nullptr;
	}

	CommonErrorCode Orchestrator::IsExistStreamInOriginMapStore(const info::VHostAppName &vhost_app_name, const ov::String &stream_name) const
	{
		auto vhost = GetVirtualHost(vhost_app_name);
		if (vhost == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		if (vhost->is_origin_map_store_enabled == false)
		{
			// disabled by user
			return CommonErrorCode::DISABLED;
		}
		
		auto client = vhost->origin_map_client;
		if (client == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		auto app_stream_name = ov::String::FormatString("%s/%s", vhost_app_name.GetAppName().CStr(), stream_name.CStr());

		ov::String temp_str;
		return client->GetOrigin(app_stream_name, temp_str);
	}

	std::shared_ptr<ov::Url> Orchestrator::GetOriginUrlFromOriginMapStore(const info::VHostAppName &vhost_app_name, const ov::String &stream_name) const
	{
		auto vhost = GetVirtualHost(vhost_app_name);
		if (vhost == nullptr)
		{
			// Error
			return nullptr;
		}

		if (vhost->is_origin_map_store_enabled == false)
		{
			// disabled by user
			return nullptr;
		}
		
		auto client = vhost->origin_map_client;
		if (client == nullptr)
		{
			// Error
			return nullptr;
		}

		auto app_stream_name = ov::String::FormatString("%s/%s", vhost_app_name.GetAppName().CStr(), stream_name.CStr());

		ov::String url_str;
		if (client->GetOrigin(app_stream_name, url_str) == CommonErrorCode::SUCCESS)
		{
			return ov::Url::Parse(url_str);
		}

		return nullptr;
	}

	CommonErrorCode Orchestrator::RegisterStreamToOriginMapStore(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		auto vhost = GetVirtualHost(vhost_app_name);
		if (vhost == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		if (vhost->is_origin_map_store_enabled == false)
		{
			// disabled by user
			return CommonErrorCode::DISABLED;
		}
		
		auto client = vhost->origin_map_client;
		if (client == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		auto app_stream_name = ov::String::FormatString("%s/%s", vhost_app_name.GetAppName().CStr(), stream_name.CStr());
		auto ovt_url = ov::String::FormatString("%s/%s", vhost->origin_base_url.CStr(), app_stream_name.CStr());
		if (client->Register(app_stream_name, ovt_url) == true)
		{
			return CommonErrorCode::SUCCESS;
		}

		return CommonErrorCode::ERROR;
	}

	CommonErrorCode Orchestrator::UnregisterStreamFromOriginMapStore(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		auto vhost = GetVirtualHost(vhost_app_name);
		if (vhost == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		if (vhost->is_origin_map_store_enabled == false)
		{
			// disabled by user
			return CommonErrorCode::DISABLED;
		}
		
		auto client = vhost->origin_map_client;
		if (client == nullptr)
		{
			// Error
			return CommonErrorCode::ERROR;
		}

		auto app_stream_name = ov::String::FormatString("%s/%s", vhost_app_name.GetAppName().CStr(), stream_name.CStr());

		if (client->Unregister(app_stream_name) == true)
		{
			return CommonErrorCode::SUCCESS;
		}

		return CommonErrorCode::ERROR;
	}

	// This feature is set in Application.PersistentStream. Creates a persistent and non-terminating stream based on the input stream. 
	// If the input stream is terminated, it is played as a fallback stream.
	// - Create a persistent stream only for the input stream.
	// - Main stream will use Input Stream. The fallback stream will use the stream specified in PersistentStreams.Stream.FallbackStreamName. It can only be used within the same application.
	// - The created Persistent Stream can be terminated when the API(TODO) or Application is deleted.
	// - Internally it uses multiple urls from OVT provider/publisher within localhost.
	CommonErrorCode Orchestrator::CreatePersistentStreamIfNeed(const info::Application &app_info, const std::shared_ptr<info::Stream> &stream_info)
	{
		auto vhost = GetVirtualHost(app_info.GetName());
		if(!vhost)
		{
			return CommonErrorCode::NOT_FOUND;
		}

		// Streams created by OVT, FILE or Transcoder Provider are excluded.
		if(stream_info->GetSourceType() == StreamSourceType::Ovt || stream_info->GetSourceType() == StreamSourceType::File || stream_info->GetSourceType() == StreamSourceType::Transcoder)
		{
			return CommonErrorCode::DISABLED;
		}

		auto persist_streams = app_info.GetConfig().GetPersistentStreams();
		for(auto conf : persist_streams.GetStreams())
		{
			auto ovt_scheme = ov::String("ovt");
			auto ovt_port = _server_config->GetBind().GetPublishers().GetOvt().GetPort().GetPort();

			auto vhost_name = app_info.GetName().GetVHostName();
			auto app_name = app_info.GetName().GetAppName();

			auto source_stream_match = conf.GetOriginStreamName();
			auto source_stream_name = stream_info->GetName();

			auto fallback_stream_name = conf.GetFallbackStreamName();

			auto new_stream_name = conf.GetName().Replace("${OriginStreamName}", source_stream_name.CStr());

			// Check that the created stream matches the SourceStreamMatch value
			ov::Regex regex(ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(source_stream_match)));
			if(regex.Matches(source_stream_name.CStr()).IsMatched() == false)
			{
				continue;
			}

			// Set url of main/fallback stream
			std::vector<ov::String> url_list;
			url_list.push_back(ov::String::FormatString("%s://localhost:%d/%s/%s", ovt_scheme.CStr(), ovt_port, app_name.CStr(), source_stream_name.CStr(), vhost_name.CStr())); // Main
			url_list.push_back(ov::String::FormatString("%s://localhost:%d/%s/%s", ovt_scheme.CStr(), ovt_port, app_name.CStr(), fallback_stream_name.CStr(), vhost_name.CStr())); // Fallback

			// Persistent = true
			// Failback = true
			// Relay = false
			auto stream_props = std::make_shared<pvd::PullStreamProperties>();
			stream_props->SetPersistent(true);
			stream_props->SetFailback(true);
			stream_props->SetRelay(false);

 			// Request pull stream
			if( RequestPullStream(nullptr, app_info.GetName(), new_stream_name, url_list, 0, stream_props) == false)
			{
				logte("Could not create persistent stream : %s/%s", app_name.CStr(), new_stream_name.CStr());
				return CommonErrorCode::ERROR;
			}
		}

		return CommonErrorCode::SUCCESS;
	}
}  // namespace ocst