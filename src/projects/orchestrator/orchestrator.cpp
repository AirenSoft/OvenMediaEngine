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

namespace ocst
{
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

				logtd("    - Processing for hosts: %d items", host_info.GetHost().GetNameList().size());

				for (auto &domain_name : host_info.GetHost().GetNameList())
				{
					logtd("      - %s: New", domain_name.CStr());
					vhost->host_list.emplace_back(domain_name);
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

		return std::move(resolved);
	}

	bool Orchestrator::GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list)
	{
		auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

		return OrchestratorInternal::GetUrlListForLocation(vhost_app_name, host_name, stream_name, url_list, nullptr, nullptr);
	}

	ocst::Result Orchestrator::CreateApplication(const info::Host &host_info, const cfg::vhost::app::Application &app_config)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		auto vhost_name = host_info.GetName();

		info::Application app_info(host_info, GetNextAppId(), ResolveApplicationName(vhost_name, app_config.GetName()), app_config, false);

		return OrchestratorInternal::CreateApplication(vhost_name, app_info);
	}

	ocst::Result Orchestrator::DeleteApplication(const info::Application &app_info)
	{
		auto scoped_lock = std::scoped_lock(_module_list_mutex, _virtual_host_map_mutex);

		return OrchestratorInternal::DeleteApplication(app_info);
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

		return Result::Succeeded;
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

	bool Orchestrator::RequestPullStream(
		const std::shared_ptr<const ov::Url> &request_from,
		const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
		const ov::String &url, off_t offset)
	{
		auto parsed_url = ov::Url::Parse(url);

		if (parsed_url != nullptr)
		{
			// The URL has a scheme
			auto source = parsed_url->Source();

			OV_ASSERT(url == source, "url and source must be the same, but url: %s, source: %s", url.CStr(), source.CStr());

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

			auto stream = provider_module->PullStream(request_from, app_info, stream_name, {source}, offset);

			if (stream != nullptr)
			{
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

			if (request_from->HasQueryString())
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

		auto stream = provider_module->PullStream(request_from, app_info, stream_name, url_list, offset);

		if (stream != nullptr)
		{
			auto scoped_lock = std::scoped_lock(_virtual_host_map_mutex);

			auto stream_id = stream->GetId();

			auto &origin_stream_map = matched_origin->stream_map;
			auto exists_in_origin = (origin_stream_map.find(stream_id) != origin_stream_map.end());

			auto key_pair = std::pair(host_name, vhost_app_name.GetAppName());

			auto &host_stream_map = matched_host->stream_map[key_pair];
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

	bool Orchestrator::OnStreamCreated(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s stream is created", info->GetName().CStr());
		return true;
	}

	bool Orchestrator::OnStreamDeleted(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logtd("%s stream is deleted", info->GetName().CStr());
		return true;
	}

	bool Orchestrator::OnStreamPrepared(const info::Application &app_info, const std::shared_ptr<info::Stream> &info)
	{
		logte("%s stream is parsed", info->GetName().CStr());
		return true;
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
}  // namespace ocst