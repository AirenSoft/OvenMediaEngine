//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_manager.h"

#include <iostream>

#include "config_logger_loader.h"
#include "config_private.h"
#include "items/items.h"

namespace cfg
{
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

	bool ConfigManager::LoadConfigs(ov::String config_path)
	{
		if (config_path.IsEmpty())
		{
			config_path = ov::PathManager::GetAppPath("conf");
		}

		PrepareMacros();

		// Load Logger
		if (LoadLoggerConfig(config_path) == false)
		{
			return false;
		}

		ov::String server_config_path = ov::PathManager::Combine(config_path, "Server.xml");
		logti("Trying to load configurations... (%s)", server_config_path.CStr());

		_server = std::make_shared<cfg::Server>();
		auto result = _server->Parse(server_config_path, "Server");

		if (IsValidVersion("Server", ov::Converter::ToInt32(_server->GetVersion())) == false)
		{
			return false;
		}

		_config_path = config_path;

		if (result != nullptr)
		{
			logte("%s", result->ToString().CStr());
		}

		return (result == nullptr);
	}

	bool ConfigManager::LoadConfigs()
	{
		return LoadConfigs("");
	}

	bool ConfigManager::ReloadConfigs()
	{
		return LoadConfigs(_config_path);
	}

	void ConfigManager::PrepareMacros()
	{
		_macros.clear();
		_macros["${ome.AppHome}"] = ov::PathManager::GetAppPath();
		_macros["${ome.CurrentPath}"] = ov::PathManager::GetCurrentPath();
	}

	bool ConfigManager::LoadLoggerConfig(const ov::String &config_path) noexcept
	{
		struct stat value = {0};

		ov::String logger_config_path = ov::PathManager::Combine(config_path, "Logger.xml");

		::memset(&_last_modified, 0, sizeof(_last_modified));
		if (::stat(logger_config_path, &value) == -1)
		{
			// There is no file or to open file error
			// OME will work with the default settings.
			logtw("There is no configuration file for logs : %s. OME will run with the default settings.", logger_config_path.CStr());
			return true;
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
			// log.config가 변경되지 않음
			return true;
		}

		ov_log_reset_enable();

#if defined(__APPLE__)
		_last_modified = value.st_mtimespec;
#else
		_last_modified = value.st_mtim;
#endif

		auto logger_loader = std::make_shared<ConfigLoggerLoader>(logger_config_path);
		if (logger_loader == nullptr)
		{
			logtc("Failed to load config Logger.xml");
			return false;
		}

		if (!logger_loader->Parse())
		{
			// Logger.xml 파싱에 실패한 경우
			return false;
		}

		if (IsValidVersion("Logger", ov::Converter::ToInt32(logger_loader->GetVersion())) == false)
		{
			return false;
		}

		auto log_path = logger_loader->GetLogPath();
		ov_log_set_path(log_path.CStr());

		// Init stat log
		//TODO(Getroot): This is temporary code for testing. This will change to more elegant code in the future.
		ov_stat_log_set_path(STAT_LOG_WEBRTC_EDGE, log_path.CStr());
		ov_stat_log_set_path(STAT_LOG_HLS_EDGE_SESSION, log_path.CStr());
		ov_stat_log_set_path(STAT_LOG_HLS_EDGE_REQUEST, log_path.CStr());
		ov_stat_log_set_path(STAT_LOG_HLS_EDGE_VIEWERS, log_path.CStr());

		logti("Trying to set logfile in directory... (%s)", log_path.CStr());

		std::vector<std::shared_ptr<LoggerTagInfo>> tags = logger_loader->GetTags();
		for (auto iterator = tags.begin(); iterator != tags.end(); ++iterator)
		{
			auto name = (*iterator)->GetName();
			if (ov_log_set_enable(name.CStr(), (*iterator)->GetLevel(), true) == false)
			{
				logtc("Could not set log level for tag: %s", name.CStr());

				return false;
			}
		}

		logger_loader->Reset();
		return true;
	}

	ov::String ConfigManager::ResolveMacros(ov::String string)
	{
		for (auto macro : _macros)
		{
			string = string.Replace(macro.first, macro.second);
		}

		return string;
	}

	bool ConfigManager::IsValidVersion(const ov::String &name, int version)
	{
		auto supported_xml = _supported_xml.find(name);

		if (supported_xml == _supported_xml.end())
		{
			logtc("Cannot find conf XML (%s.xml)", name.CStr());
			return false;
		}

		auto supported_version = supported_xml->second;

		if (version == 0)
		{
			logtc("Unknown configuration found in your XML.",
				  name.CStr(), version, supported_version);

			logtc("If you have upgraded OME, see misc/conf_examples/%s.xml",
				  name.CStr());
		}
		else if (version != supported_version)
		{
			logtc("The version of %s.xml is outdated (Your XML version: %d, Latest version: %d).",
				  name.CStr(), version, supported_version);

			logtc("If you have upgraded OME, see misc/conf_examples/%s.xml",
				  name.CStr());

			if ((version == 7) && (supported_version == 8))
			{
				logtc("Major Changes (v7 -> v8):");
				logtc(" - Added <Server>.<Bind>.<Managers>.<API> for setting API binding port");
				logtc(" - Added <Server>.<API> for setting API server");
				logtc(" - Added <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<OutputProfiles>");
				logtc(" - Changed <Server>.<VirtualHosts>.<VirtualHost>.<Domain> to <Host>");
				logtc(" - Changed <CrossDomain> to <CrossDomains>");
				logtc(" - Deleted <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<Streams>");
				logtc(" - Deleted <Server>.<VirtualHosts>.<VirtualHost>.<Applications>.<Application>.<Encodes>");

				return false;
			}
		}

		return true;
	}
}  // namespace cfg