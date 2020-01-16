//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "data_structure.h"

#include <regex>

#include <base/provider/provider.h>

//
// Orchestrator is responsible for passing commands to registered modules, such as Provider/MediaRouter/Transcoder/Publisher.
//
// Orchestrator will upgrade to perform the following roles:
//
// 1. The publisher can request the provider to create a stream.
// 2. Other modules may request Provider/Publisher traffic information. (Especially, it will be used by the RESTful API server)
// 3. Create or manage new applications.
//    For example, if some module calls Orchestrator::CreateApplication(), the Orchestrator will create a new app
//    using the APIs of Providers, MediaRouter, and Publishers as appropriate.
//
// TODO(dimiden): Modification is required so that the module can be managed per Host
class Orchestrator
{
public:
	enum class Result
	{
		// An error occurred
		Failed,
		// Created successfully
		Succeeded,
		// The item does exists
		Exists,
		// The item does not exists
		NotExists
	};

	static Orchestrator *GetInstance()
	{
		static Orchestrator orchestrator;

		return &orchestrator;
	}

	bool ApplyOriginMap(const std::vector<cfg::VirtualHost> &vhost_list);

	/// Register the module
	///
	/// @param module Module to register
	///
	/// @return If the module is registered or passed a different type from the previously registered type, false is returned.
	/// Otherwise, true is returned.
	bool RegisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module);

	/// Unregister the module
	///
	/// @param module Module to unregister
	///
	/// @return If the module is not already registered, false is returned. Otherwise, true is returned.
	bool UnregisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module);

	ov::String GetVhostNameFromDomain(const ov::String &domain_name);

	/// Generate an application name for vhost/app
	///
	/// @param vhost_name A name of VirtualHost
	/// @param app_name An application name
	///
	/// @return A new application name corresponding to vhost/app
	ov::String ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name);

	///  Generate an application name for domain/app
	///
	/// @param domain_name A name of the domain
	/// @param app_name An application name
	///
	/// @return A new application name corresponding to domain/app
	ov::String ResolveApplicationNameFromDomain(const ov::String &domain_name, const ov::String &app_name);

	/// Create an application and notify the modules
	///
	/// @param vhost_name A name of VirtualHost
	/// @param app_config Application configuration to create
	///
	/// @return Creation result
	///
	/// @note Automatically DeleteApplication() when application creation fails
	Result CreateApplication(const ov::String &vhost_name, const cfg::Application &app_config);
	/// Delete the application and notify the modules
	///
	/// @param app_info Application information to delete
	///
	/// @return
	///
	/// @note If an error occurs during deletion, do not recreate the application
	Result DeleteApplication(const info::Application &app_info);

	const info::Application &GetApplication(const ov::String &app_name) const;
	const info::Application &GetApplication(info::application_id_t app_id) const;

	// bool RequestPullStream(const ov::String &url);
	bool RequestPullStream(const ov::String &application, const ov::String &stream, off_t offset);
	bool RequestPullStream(const ov::String &application, const ov::String &stream)
	{
		return RequestPullStream(application, stream, 0);
	}

protected:
	struct Module
	{
		Module(OrchestratorModuleType type, const std::shared_ptr<OrchestratorModuleInterface> &module)
			: type(type),
			  module(module)
		{
		}

		OrchestratorModuleType type = OrchestratorModuleType::Unknown;
		std::shared_ptr<OrchestratorModuleInterface> module = nullptr;
	};

	enum class ItemState
	{
		Unknown,
		// This item is applied to OriginMap
		Applied,
		// This item is applied, and not changed
		NotChanged,
		// This item is not applied, and will be applied to OriginMap/OriginList
		New,
		// This item is applied, but need to change some values
		Changed,
		// This item is applied, and will be deleted from OriginMap/OriginList
		Delete
	};

	struct Origin
	{
		static const Origin &InvalidValue()
		{
			static Origin invalid_value;

			return invalid_value;
		}

		bool IsValid() const
		{
			return state != ItemState::Unknown;
		}

		Origin(const cfg::OriginsOrigin &origin_config)
			: location(origin_config.GetLocation()),
			  scheme(origin_config.GetPass().GetScheme()),
			  state(ItemState::New)

		{
			for (auto &item : origin_config.GetPass().GetUrlList())
			{
				auto url = item.GetUrl();

				// Prepend "<scheme>://"
				url.Prepend("://");
				url.Prepend(scheme);

				url_list.push_back(item.GetUrl());
			}

			this->origin_config = origin_config;
		}

		info::application_id_t app_id = 0U;

		ov::String scheme;

		// Origin/Location
		ov::String location;
		// Generated URL list from <Origin>.<Pass>.<URL>
		std::vector<ov::String> url_list;

		// Original configuration
		cfg::OriginsOrigin origin_config;

		// A flag used by ApplyOriginMap() to determine if an item has changed
		mutable ItemState state = ItemState::Unknown;

	private:
		Origin() = default;
	};

	struct Domain
	{
		Domain(const ov::String &vhost_name, const ov::String &name, const std::vector<Origin> &origin_list)
			: vhost_name(vhost_name),
			  name(name),
			  origin_list(origin_list),
			  state(ItemState::New)
		{
			UpdateRegex();
		}

		bool UpdateRegex()
		{
			// Escape special characters
			auto special_characters = std::regex(R"([[\\./+{}$^|])");
			ov::String escaped = std::regex_replace(name.CStr(), special_characters, R"(\$&)").c_str();
			// Change "*"/"?" to ".*"/".?"
			escaped = escaped.Replace("*", ".*");
			escaped = escaped.Replace("?", ".?");
			escaped.Prepend("^");
			escaped.Append("$");

			try
			{
				regex_for_domain = std::regex(escaped);
			}
			catch (std::exception &e)
			{
				return false;
			}

			return true;
		}

		// The name of VirtualHost (eg: AirenSoft-VHost)
		ov::String vhost_name;

		// The name of Domain (eg: *.airensoft.com)
		ov::String name;
		std::regex regex_for_domain;

		std::vector<Origin> origin_list;

		// A flag used by ApplyOriginMap() to determine if an item has changed
		ItemState state = ItemState::Unknown;
	};

	Orchestrator() = default;

	/// Compares a list of origin and adds them to added_origin_list if a new entry is found
	///
	/// @param origin_list The origin list
	/// @param new_origin_list Origin to compare
	/// @param added_origin_list Added origin list
	///
	/// @return Whether there are items added/modified/deleted
	bool ProcessOriginList(const std::vector<Origin> &origin_list, const std::vector<Origin> &new_origin_list, std::vector<Origin> *added_origin_list) const;
	/// Compares a list of domains and adds them to added_domain_list if a new entry is found
	///
	/// @param domain_list The domain list
	/// @param vhost VirtualHost configuration to compare
	/// @param added_domain_list Added domain list
	///
	/// @return Whether there are items added/modified/deleted
	bool ProcessDomainList(std::vector<Domain> *domain_list, const cfg::VirtualHost &vhost, std::vector<Domain> *added_domain_list) const;

	info::application_id_t GetNextAppId();

	std::shared_ptr<pvd::Provider> GetProviderForScheme(const ov::String &scheme);
	std::shared_ptr<OrchestratorProviderModuleInterface> GetProviderModuleForScheme(const ov::String &scheme);
	std::shared_ptr<pvd::Provider> GetProviderForUrl(const ov::String &url);

	const Orchestrator::Origin &GetUrlListForLocation(const ov::String &app_name, const ov::String &stream_name, std::vector<ov::String> *url_list);

	Result CreateApplicationInternal(const info::Application &app_info);
	Result CreateApplicationInternal(const ov::String &name, info::Application *app_info);

	Result NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info);
	Result DeleteApplicationInternal(info::application_id_t app_id);
	Result DeleteApplicationInternal(const info::Application &app_info);

	const info::Application &GetApplicationInternal(const ov::String &app_name) const;
	const info::Application &GetApplicationInternal(info::application_id_t app_id) const;

	// bool RequestPullStreamForUrl(const std::shared_ptr<const ov::Url> &url);
	bool RequestPullStreamForLocation(const ov::String &app_name, const ov::String &stream_name, off_t offset);

	info::application_id_t _last_application_id = info::MinApplicationId;

	// Origin map
	std::recursive_mutex _domain_list_mutex;
	std::vector<Domain> _domain_list;

	// Modules
	std::recursive_mutex _modules_mutex;
	std::vector<Module> _modules;
	std::map<OrchestratorModuleType, std::vector<std::shared_ptr<OrchestratorModuleInterface>>> _module_map;

	// Application list
	mutable std::recursive_mutex _app_map_mutex;
	std::map<info::application_id_t, info::Application> _app_map;
};