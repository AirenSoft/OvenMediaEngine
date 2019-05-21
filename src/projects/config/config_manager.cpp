//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_manager.h"

#include "items/items.h"

#include "config_private.h"
#include "config_logger_loader.h"

#include <iostream>

namespace cfg
{
	ConfigManager::ConfigManager()
	{
	    // Modify if supported xml version is added or changed
        _supported_xml.insert(
                std::make_pair("Server",std::vector<std::string>({"2", "2.0"}))
                );

        _supported_xml.insert(
                std::make_pair("Logger", std::vector<std::string>({"2", "2.0"}))
                );
	}

	ConfigManager::~ConfigManager()
	{
	}

	bool ConfigManager::LoadConfigs(ov::String config_path)
	{
		if(config_path.IsEmpty())
		{
			config_path = ov::PathManager::GetAppPath("conf");
		}

		PrepareMacros();

		// Load Logger
		if(LoadLoggerConfig(config_path) == false)
		{
			return false;
		}

		ov::String server_config_path = ov::PathManager::Combine(config_path, "Server.xml");
		logti("Trying to load configurations... (%s)", server_config_path.CStr());

		_server = std::make_shared<cfg::Server>();
        bool result = _server->Parse(server_config_path, "Server");

        if (IsValidVersion("Server", _server->GetVersion().CStr()) == false)
        {
            return false;
        }

		return result;
	}

	bool ConfigManager::LoadConfigs()
	{
		return LoadConfigs("");
	}

	void ConfigManager::PrepareMacros()
	{
		_macros.clear();
		_macros["${ome.AppHome}"] = ov::PathManager::GetAppPath();
		_macros["${ome.CurrentPath}"] = ov::PathManager::GetCurrentPath();
	}

	bool ConfigManager::LoadLoggerConfig(const ov::String &config_path) noexcept
	{
		struct stat value = { 0 };

		ov::String logger_config_path = ov::PathManager::Combine(config_path, "Logger.xml");

		::memset(&_last_modified, 0, sizeof(_last_modified));
		::stat(logger_config_path, &value);

		if(
			(_last_modified.tv_sec == value.st_mtim.tv_sec) &&
			(_last_modified.tv_nsec == value.st_mtim.tv_nsec)
			)
		{
			// log.config가 변경되지 않음
			return true;
		}

		ov_log_reset_enable();

		_last_modified = value.st_mtim;

		auto logger_loader = std::make_shared<ConfigLoggerLoader>(logger_config_path);
		if(logger_loader == nullptr)
		{
			logte("Failed to load config Logger.xml");
			return false;
		}

		if(!logger_loader->Parse())
		{
			// Logger.xml 파싱에 실패한 경우
			return false;
		}

		if (IsValidVersion("Logger", logger_loader->GetVersion().c_str()) == false)
        {
		    return false;
        }

        auto log_path = logger_loader->GetLogPath();
        ov_log_set_path(log_path.c_str());
        logti("Trying to save logfile in directory... (%s)", log_path.c_str());

		std::vector<std::shared_ptr<LoggerTagInfo>> tags = logger_loader->GetTags();
		for(auto iterator = tags.begin(); iterator != tags.end(); ++iterator)
		{
			ov_log_set_enable((*iterator)->GetName().CStr(), (*iterator)->GetLevel(), true);
		}

		logger_loader->Reset();
		return true;
	}

	ov::String ConfigManager::ResolveMacros(ov::String string)
	{
		for(auto macro : _macros)
		{
			string = string.Replace(macro.first, macro.second);
		}

		return string;
	}

	bool ConfigManager::IsValidVersion(const std::string& name, const std::string& version)
    {
        auto supported_xml = _supported_xml.find(name);
        if(supported_xml == _supported_xml.end())
        {
            logte("Cannot find conf XML (%s.xml)", name.c_str());
            return false;
        }

        auto supported_version = supported_xml->second;
        if (std::find(supported_version.begin(), supported_version.end(), version) != supported_version.end())
        {
            return true;
        }

        logte("The version of %s.xml is incorrect. If you have upgraded OME, see misc/conf_examples/%s.xml",
                name.c_str(),
                name.c_str());

	    return false;
    }
}
