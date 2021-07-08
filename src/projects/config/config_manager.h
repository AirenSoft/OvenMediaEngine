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

#define CFG_LOG_FILE_NAME "Logger.xml"
#define CFG_MAIN_FILE_NAME "Server.xml"
#define CFG_LAST_CONFIG_FILE_NAME "LastConfig.json"

namespace cfg
{
	class ConfigManager : public ov::Singleton<ConfigManager>
	{
	public:
		friend class ov::Singleton<ConfigManager>;
		~ConfigManager() override;

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadConfigs(ov::String config_path, bool ignore_last_config);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ReloadConfigs();

		Json::Value GetCurrentConfigAsJson();

		// ConfigManager contains only the configurations when OME first runs,
		// so if you want to save the last changes modified with RESTful API, you need to call this API.
		// (DO NOT USE GetServer()->ToJson() to save configurations)
		bool SaveCurrentConfig();

		std::shared_ptr<Server> GetServer() noexcept
		{
			return _server;
		}

		ov::String GetConfigPath()
		{
			return _config_path;
		}

	protected:
		ConfigManager();

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadLoggerConfig(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadServerID(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void CheckValidVersion(const ov::String &name, int version);

		ov::String _config_path;
		bool _ignore_last_config = false;

		std::shared_ptr<Server> _server;
		ov::String _server_id;
		std::map<ov::String, ov::String> _macros;

		timespec _last_modified;

		// key: XML file name
		// value: version number
		std::map<ov::String, int> _supported_xml;

		std::mutex _config_mutex;
	
	private:
		std::tuple<bool, ov::String> LoadServerIDFromStorage(const ov::String &config_path) const;
		bool StoreServerID(const ov::String &config_path, ov::String server_id);
		std::tuple<bool, ov::String> GenerateServerID() const;
	};
}  // namespace cfg
