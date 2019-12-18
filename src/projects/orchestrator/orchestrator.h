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
		// The item is exists
		Exists,
		// The item is not exists
		NotExists
	};

	static Orchestrator *GetInstance()
	{
		static Orchestrator orchestrator;

		return &orchestrator;
	}

	bool PrepareOriginMap(const cfg::Origins &origins);

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
	/// @param app_conf Application configuration to create
	///
	/// @return Creation result
	///
	/// @note Automatically DeleteApplication() when application creation fails
	Result CreateApplication(const cfg::Application &app_conf);
	/// Delete the application and notify the modules
	///
	/// @param app_info Application information to delete
	///
	/// @return
	///
	/// @note If an error occurs during deletion, do not recreate the application
	Result DeleteApplication(const info::Application &app_info);

	bool RequestPullStream(const ov::String &url);
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

		info::application_id_t app_id = 0U;

		ov::String scheme;

		// Origin/Location
		ov::String location;
		// Origin/Pass/Url
		std::vector<ov::String> url_list;
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

	Orchestrator() = default;

	info::application_id_t GenerateApplicationId() const;

	std::shared_ptr<pvd::Provider> GetProviderForScheme(const ov::String &scheme);
	std::shared_ptr<pvd::Provider> GetProviderForUrl(const ov::String &url);

	std::shared_ptr<Origin> GetUrlListForLocation(const ov::String &location, ov::String *app_name, ov::String *stream_name, std::vector<ov::String> *url_list);

	Result CreateApplicationInternal(const ov::String &name, info::Application *app_info = nullptr);
	Result CreateApplicationInternal(const info::Application &app_info);

	Result DeleteApplicationInternal(info::application_id_t app_id);
	Result DeleteApplicationInternal(const info::Application &app_info);

	bool RequestPullStreamForUrl(const std::shared_ptr<const ov::Url> &url);
	bool RequestPullStreamForLocation(const ov::String &location);

	// Origin map
	std::mutex _origin_map_mutex;
	std::vector<std::shared_ptr<Origin>> _origin_list;

	// Modules
	std::mutex _modules_mutex;
	std::vector<Module> _modules;
	std::map<OrchestratorModuleType, std::vector<std::shared_ptr<OrchestratorModuleInterface>>> _module_map;

	// Application list
	std::mutex _app_map_mutex;
	std::map<info::application_id_t, info::Application> _app_map;
};