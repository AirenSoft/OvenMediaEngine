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

	bool ApplyOriginMap(const std::vector<cfg::VirtualHost> &host_list);
	bool ApplyOriginMap(const cfg::Domain &domain, const cfg::Origins &origins);

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

	/// Create an application and notify the modules
	///
	/// @param app_config Application configuration to create
	///
	/// @return Creation result
	///
	/// @note Automatically DeleteApplication() when application creation fails
	Result CreateApplication(const cfg::Application &app_config);
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
	bool RequestPullStream(const ov::String &application, const ov::String &stream);

protected:
	struct Origin
	{
		Origin(const cfg::OriginsOrigin &origin)
		{
			location = origin.GetLocation();
			scheme = origin.GetPass().GetScheme();

			for (auto &item : origin.GetPass().GetUrlList())
			{
				auto url = item.GetUrl();

				// Prepend "<scheme>://"
				url.Prepend("://");
				url.Prepend(scheme);

				url_list.push_back(item.GetUrl());
			}
		}

		bool operator==(const Origin &origin) const
		{
			auto this_list = origins.GetOriginList();
			auto target_list = origin.origins.GetOriginList();

			// Compare two cfg::Origins

			if (this_list.size() != target_list.size())
			{
				return false;
			}

			return std::equal(
				this_list.begin(), this_list.end(), target_list.begin(),
				[](const cfg::OriginsOrigin &first, const cfg::OriginsOrigin &second) -> bool {
					if (first.GetLocation() != second.GetLocation())
					{
						return false;
					}

					auto &first_pass = first.GetPass();
					auto &second_pass = second.GetPass();

					if (first_pass.GetScheme() != second_pass.GetScheme())
					{
						return false;
					}

					auto &first_url_list = first_pass.GetUrlList();
					auto &second_url_list = second_pass.GetUrlList();

					if (first_url_list.size() != second_url_list.size())
					{
						return false;
					}

					if (std::equal(first_url_list.begin(), first_url_list.end(), second_url_list.begin(),
								   [](const cfg::Url &url1, const cfg::Url &url2) -> bool {
									   return url1.GetUrl() == url2.GetUrl();
								   }) == false)
					{
						return false;
					}
				});
		}

		info::application_id_t app_id = 0U;

		ov::String scheme;

		// Origin/Location
		ov::String location;
		// Generated URL list from <Origin>.<Pass>.<URL>
		std::vector<ov::String> url_list;

		// Original configuration
		cfg::Origins origins;
	};

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

	enum class DomainItemState
	{
		Unknown,
		// This item is applied to OriginMap
		Applied,
		// This item is applied, and not changed
		NotChanged,
		// This item is not applied, and will be applied to OriginMap
		New,
		// This item is applied, but need to change some values of cfg::Origins
		Changed,
		// This item is applied, and will be deleted from OriginMap
		Delete
	};

	struct Domain
	{
		Domain(const ov::String &name, const std::vector<std::shared_ptr<const Origin>> &origin_list)
			: name(name),
			  origin_list(origin_list),
			  state(DomainItemState::New)
		{
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

		ov::String name;
		std::regex regex_for_domain;

		std::vector<std::shared_ptr<const Origin>> origin_list;

		// A flag used by ApplyOriginMap() to determine if an item has changed
		DomainItemState state = DomainItemState::Unknown;
	};

	Orchestrator() = default;

	info::application_id_t GetNextAppId();

	std::shared_ptr<pvd::Provider> GetProviderForScheme(const ov::String &scheme);
	std::shared_ptr<OrchestratorProviderModuleInterface> GetProviderModuleForScheme(const ov::String &scheme);
	std::shared_ptr<pvd::Provider> GetProviderForUrl(const ov::String &url);

	std::shared_ptr<const Orchestrator::Origin> GetUrlListForLocation(const ov::String &app_name, const ov::String &stream_name, std::vector<ov::String> *url_list);

	Result CreateApplicationInternal(const info::Application &app_info);
	Result CreateApplicationInternal(const ov::String &name, info::Application *app_info);

	Result NotifyModulesForDeleteEvent(const std::vector<Module> &modules, const info::Application &app_info);
	Result DeleteApplicationInternal(info::application_id_t app_id);
	Result DeleteApplicationInternal(const info::Application &app_info);

	const info::Application &GetApplicationInternal(const ov::String &app_name) const;
	const info::Application &GetApplicationInternal(info::application_id_t app_id) const;

	// bool RequestPullStreamForUrl(const std::shared_ptr<const ov::Url> &url);
	bool RequestPullStreamForLocation(const ov::String &app_name, const ov::String &stream_name);

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