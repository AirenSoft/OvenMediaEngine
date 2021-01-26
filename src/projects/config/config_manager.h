//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "items/items.h"

namespace cfg
{
	class ConfigManager : public ov::Singleton<ConfigManager>
	{
	public:
		friend class ov::Singleton<ConfigManager>;
		~ConfigManager() override;

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadConfigs(ov::String config_path);

		// Load configs from default path (<binary_path>/conf/*)
		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadConfigs();

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ReloadConfigs();

		std::shared_ptr<Server> GetServer() noexcept
		{
			return _server;
		}

	protected:
		ConfigManager();

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadLoggerConfig(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void CheckValidVersion(const ov::String &name, int version);

		ov::String _config_path;

		std::shared_ptr<Server> _server;
		std::map<ov::String, ov::String> _macros;

		timespec _last_modified;

		// key: XML file name
		// value: version number
		std::map<ov::String, int> _supported_xml;
	};
}  // namespace cfg
