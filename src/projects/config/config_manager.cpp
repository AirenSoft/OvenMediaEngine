//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_manager.h"

#include <monitoring/monitoring.h>

#include <iostream>

#include "config_converter.h"
#include "config_logger_loader.h"
#include "config_private.h"
#include "items/items.h"

namespace cfg
{
	struct XmlWriter : pugi::xml_writer
	{
		ov::String result;

		void write(const void *data, size_t size) override
		{
			result.Append(static_cast<const char *>(data), size);
		}
	};

	ConfigManager::ConfigManager()
	{
		// Modify if supported xml version is added or changed

		// Version 7 -> 8
		_supported_xml["Server"] = 8;
		_supported_xml["Logger"] = 2;
	}

	ConfigManager::~ConfigManager()
	{
	}

	void ConfigManager::LoadConfigs(ov::String config_path, bool ignore_last_config)
	{
		if (config_path.IsEmpty())
		{
			config_path = ov::PathManager::GetAppPath("conf");
		}

		// Load Logger
		LoadLoggerConfig(config_path);

		bool read_from_main_file = true;
		const char *ROOT_NAME = "Server";

		// Try to read configurations from CFG_LAST_CONFIG_FILE_NAME
		ov::String last_config_path = ov::PathManager::Combine(config_path, CFG_LAST_CONFIG_FILE_NAME);
		if (ignore_last_config == false)
		{
			if (ov::PathManager::IsFile(last_config_path))
			{
				// Read from last config
				logti("Trying to load configurations from last config... (%s)", last_config_path.CStr());

				try
				{
					DataSource data_source(DataType::Json, config_path, last_config_path, ROOT_NAME);
					_server = std::make_shared<cfg::Server>();
					_server->FromDataSource("server", ROOT_NAME, data_source);

					read_from_main_file = false;
				}
				catch (const std::shared_ptr<ConfigError> &error)
				{
					logte("Could not load configurations from last config. (Is file corrupted?): %s", error->ToString().CStr());
				}
			}
		}
		else
		{
			if (ov::PathManager::IsFile(last_config_path))
			{
				logtw("Last config is ignored by option");
			}
		}

		if (read_from_main_file)
		{
			ov::String server_config_path = ov::PathManager::Combine(config_path, CFG_MAIN_FILE_NAME);

			logti("Trying to load configurations... (%s)", server_config_path.CStr());

			DataSource data_source(DataType::Xml, config_path, server_config_path, ROOT_NAME);
			_server = std::make_shared<cfg::Server>();
			_server->FromDataSource("Server", ROOT_NAME, data_source);
		}

		CheckValidVersion("Server", ov::Converter::ToInt32(_server->GetVersion()));

		_config_path = config_path;
		_ignore_last_config = ignore_last_config;
	}

	void ConfigManager::ReloadConfigs()
	{
		LoadConfigs(_config_path, _ignore_last_config);
	}

	Json::Value ConfigManager::GetCurrentConfigAsJson()
	{
		auto lock_guard = std::lock_guard(_config_mutex);

		auto config = conv::GetServerJsonFromConfig(_server, false);

		return std::move(config);
	}

	bool ConfigManager::SaveCurrentConfig()
	{
		Json::Value config = GetCurrentConfigAsJson();

		ov::String last_config_path = ov::PathManager::Combine(_config_path, CFG_LAST_CONFIG_FILE_NAME);
		auto config_json = config.toStyledString();

		auto file = ov::DumpToFile(last_config_path, config_json.c_str(), config_json.length());

		if (file == nullptr)
		{
			logte("Could not write config to file: %s", last_config_path.CStr());
			return false;
		}

		logti("Current config is written to %s", last_config_path.CStr());

		return true;
	}

	void ConfigManager::LoadLoggerConfig(const ov::String &config_path)
	{
		struct stat value = {0};

		ov::String logger_config_path = ov::PathManager::Combine(config_path, CFG_LOG_FILE_NAME);

		::memset(&_last_modified, 0, sizeof(_last_modified));
		if (::stat(logger_config_path, &value) == -1)
		{
			// There is no file or to open file error
			// OME will work with the default settings.
			logtw("There is no configuration file for logs : %s. OME will run with the default settings.", logger_config_path.CStr());
			return;
		}

		if (
#if defined(__APPLE__)
			(_last_modified.tv_sec == value.st_mtimespec.tv_sec) &&
			(_last_modified.tv_nsec == value.st_mtimespec.tv_nsec)
#else
			(_last_modified.tv_sec == value.st_mtim.tv_sec) &&
			(_last_modified.tv_nsec == value.st_mtim.tv_nsec)
#endif
		)
		{
			// File is not changed
			return;
		}

		::ov_log_reset_enable();

#if defined(__APPLE__)
		_last_modified = value.st_mtimespec;
#else
		_last_modified = value.st_mtim;
#endif

		auto logger_loader = std::make_shared<ConfigLoggerLoader>(logger_config_path);

		logger_loader->Parse();

		CheckValidVersion("Logger", ov::Converter::ToInt32(logger_loader->GetVersion()));

		auto log_path = logger_loader->GetLogPath();
		::ov_log_set_path(log_path.CStr());

		// Init stat log
		//TODO(Getroot): This is temporary code for testing. This will change to more elegant code in the future.
		::ov_stat_log_set_path(STAT_LOG_WEBRTC_EDGE_SESSION, log_path.CStr());
		::ov_stat_log_set_path(STAT_LOG_WEBRTC_EDGE_REQUEST, log_path.CStr());
		::ov_stat_log_set_path(STAT_LOG_WEBRTC_EDGE_VIEWERS, log_path.CStr());
		::ov_stat_log_set_path(STAT_LOG_HLS_EDGE_SESSION, log_path.CStr());
		::ov_stat_log_set_path(STAT_LOG_HLS_EDGE_REQUEST, log_path.CStr());
		::ov_stat_log_set_path(STAT_LOG_HLS_EDGE_VIEWERS, log_path.CStr());

		logti("Trying to set logfile in directory... (%s)", log_path.CStr());

		std::vector<std::shared_ptr<LoggerTagInfo>> tags = logger_loader->GetTags();

		for (auto iterator = tags.begin(); iterator != tags.end(); ++iterator)
		{
			auto name = (*iterator)->GetName();

			if (::ov_log_set_enable(name.CStr(), (*iterator)->GetLevel(), true) == false)
			{
				throw CreateConfigError("Could not set log level for tag: %s", name.CStr());
			}
		}

		logger_loader->Reset();
	}

	void ConfigManager::CheckValidVersion(const ov::String &name, int version)
	{
		auto supported_xml = _supported_xml.find(name);

		if (supported_xml == _supported_xml.end())
		{
			throw CreateConfigError("Cannot find conf XML (%s.xml)", name.CStr());
		}

		auto supported_version = supported_xml->second;

		if (version == 0)
		{
			throw CreateConfigError(
				"Could not obtain version in your XML. If you have upgraded OME, see misc/conf_examples/%s.xml",
				name.CStr());
		}

		if (version != supported_version)
		{
			ov::String description;

			description.Format(
				"The version of %s.xml is outdated (Your XML version: %d, Latest version: %d).\n",
				name.CStr(), version, supported_version);

			description.AppendFormat(
				"If you have upgraded OME, see misc/conf_examples/%s.xml\n",
				name.CStr());

			if ((version == 7) && (supported_version == 8))
			{
				description.AppendFormat("Major Changes (v7 -> v8):\n");
				description.AppendFormat(" - Added <Server>.<Bind>.<Managers>.<API> for setting API binding port\n");
				description.AppendFormat(" - Added <Server>.<API> for setting API server\n");
				description.AppendFormat(" - Added <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<OutputProfiles>\n");
				description.AppendFormat(" - Changed <Server>.<VirtualHosts>.<VirtualHost>.<Domain> to <Host>\n");
				description.AppendFormat(" - Changed <CrossDomain> to <CrossDomains>\n");
				description.AppendFormat(" - Deleted <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<Streams>\n");
				description.AppendFormat(" - Deleted <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<Encodes>\n");
			}

			throw CreateConfigError("%s", description.CStr());
		}
	}
}  // namespace cfg