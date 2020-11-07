//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/host.h>
#include <base/mediarouter/media_route_interface.h>
#include <base/provider/provider.h>
#include <base/publisher/publisher.h>

#include "data_structures/data_structure.h"

namespace ocst
{
	/// OrchestratorInternal contains the internal implementation in which Orchestrator operates and excludes locking.
	/// Therefore, OrchestratorInternal should not be used directly; it must be used with Orchestrator.
	class OrchestratorInternal : public Application::CallbackInterface
	{
	protected:
		OrchestratorInternal() = default;

		bool ApplyForVirtualHost(const std::shared_ptr<VirtualHost> &virtual_host);

		/// Compares a list of hosts and adds them to added_host_list if a new entry is found
		///
		/// @param host_list The host list
		/// @param vhost VirtualHost configuration to compare
		///
		/// @return Whether it has changed
		ItemState ProcessHostList(std::vector<Host> *host_list, const cfg::cmn::Host &host_config) const;
		/// Compares a list of origin and adds them to added_origin_list if a new entry is found
		///
		/// @param origin_list The origin list
		/// @param new_origin_list Origin configuration to compare
		///
		/// @return Whether it has changed
		ItemState ProcessOriginList(std::vector<Origin> *origin_list, const cfg::vhost::orgn::Origins &origins_config) const;

		info::application_id_t GetNextAppId();

		std::shared_ptr<pvd::Provider> GetProviderForScheme(const ov::String &scheme);
		std::shared_ptr<PullProviderModuleInterface> GetProviderModuleForScheme(const ov::String &scheme);
		std::shared_ptr<pvd::Provider> GetProviderForUrl(const ov::String &url);

		/// Generate an application name for vhost/app
		///
		/// @param vhost_name A name of VirtualHost
		/// @param app_name An application name
		///
		/// @return A new application name corresponding to vhost/app
		virtual info::VHostAppName ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name) const;

		std::shared_ptr<VirtualHost> GetVirtualHost(const ov::String &vhost_name);
		std::shared_ptr<const VirtualHost> GetVirtualHost(const ov::String &vhost_name) const;
		std::shared_ptr<VirtualHost> GetVirtualHost(const info::VHostAppName &vhost_app_name);
		std::shared_ptr<const VirtualHost> GetVirtualHost(const info::VHostAppName &vhost_app_name) const;

		bool GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list, Origin **matched_origin, Host **matched_host);

		Result CreateApplication(const ov::String &vhost_name, const info::Application &app_info);
		Result CreateApplication(const info::VHostAppName &vhost_app_name, info::Application *app_info);

		Result NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info);
		Result DeleteApplication(const ov::String &vhost_name, info::application_id_t app_id);
		Result DeleteApplication(const info::Application &app_info);

		const info::Application &GetApplicationInfo(const info::VHostAppName &vhost_app_name) const;
		const info::Application &GetApplicationInfo(const ov::String &vhost_name, const ov::String &app_name) const;
		const info::Application &GetApplicationInfo(const ov::String &vhost_name, info::application_id_t app_id) const;

		std::shared_ptr<MediaRouteInterface> _media_router;

		std::atomic<info::application_id_t> _last_application_id{info::MinApplicationId};

		// Modules
		std::vector<Module> _module_list;

		// key: vhost_name
		std::map<ov::String, std::shared_ptr<VirtualHost>> _virtual_host_map;
		// ordered vhost list
		std::vector<std::shared_ptr<VirtualHost>> _virtual_host_list;
	};
}  // namespace ocst
