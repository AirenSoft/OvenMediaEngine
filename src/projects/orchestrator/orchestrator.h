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

// Singleton class
// TODO(dimiden): Modification is required so that the module can be managed per Host
class Orchestrator
{
public:
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

	bool RequestPullStream(const ov::String &url);
	bool RequestPullStream(const ov::String &application, const ov::String &stream);

protected:
	struct Origin
	{
		Origin(const cfg::OriginsOrigin &origin)
		{
			location = origin.GetLocation();
			for (auto url : origin.GetPass().GetUrlList())
			{
				url_list.push_back(url.GetUrl());
			}
		}

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

	bool RequestPullStreamUsingMap(const ov::String &location);
	bool RequestPullStreamUsingScheme(const ov::String &url);

	std::mutex _origin_map_mutex;
	std::vector<Origin> _origin_list;

	std::mutex _modules_mutex;
	std::vector<Module> _modules;
	std::map<OrchestratorModuleType, std::vector<std::shared_ptr<OrchestratorModuleInterface>>> _module_map;
};