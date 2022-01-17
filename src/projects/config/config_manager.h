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
#define CFG_LAST_CONFIG_FILE_NAME_LEGACY "LastConfig.json"
#define CFG_LAST_CONFIG_FILE_NAME "LastConfig.xml"
#define SERVER_ID_STORAGE_FILE "Server.id"

namespace cfg
{
	class ConfigManager : public ov::Singleton<ConfigManager>
	{
	public:
		friend class ov::Singleton<ConfigManager>;
		~ConfigManager() override;

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadConfigs(ov::String config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ReloadConfigs();

		Json::Value GetCurrentConfigAsJson() const;
		pugi::xml_document GetCurrentConfigAsXml() const;

		// ConfigManager contains only the configurations when OME first runs,
		// so if you want to save the last changes modified with RESTful API, you need to call this API.
		// (DO NOT USE GetServer()->ToJson() to save configurations)
		bool SaveCurrentConfig();

		std::shared_ptr<const Server> GetServer() const noexcept
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
		void CheckLegacyConfigs(ov::String config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadLoggerConfig(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadServerConfig(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadServerID(const ov::String &config_path);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void CheckValidVersion(const ov::String &name, int version);

		bool SaveCurrentConfig(pugi::xml_document &config, const ov::String &last_config_path);

		ov::String _config_path;

		std::shared_ptr<Server> _server;
		ov::String _server_id;
		std::map<ov::String, ov::String> _macros;

		timespec _last_modified;

		// key: XML file name
		// value: compatiable version numbers
		std::map<ov::String, std::vector<int>> _supported_versions_map;

		mutable std::mutex _config_mutex;

	private:
		std::tuple<bool, ov::String> LoadServerIDFromStorage(const ov::String &config_path) const;
		bool StoreServerID(const ov::String &config_path, ov::String server_id);
		std::tuple<bool, ov::String> GenerateServerID() const;
	};
}  // namespace cfg
