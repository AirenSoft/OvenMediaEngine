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
	}

	ConfigManager::~ConfigManager()
	{
	}

	bool ConfigManager::LoadConfigs()
	{
		PrepareMacros();

		// Load Logger
		if(LoadLoggerConfig() == false)
		{
			return false;
		}

		ov::String config_path = ov::PathManager::Combine(ov::PathManager::GetAppPath("conf"), "Server.xml");
		logti("Trying to load configurations... (%s)", config_path.CStr());

		_server = std::make_shared<cfg::Server>();


		return _server->Parse(config_path, "Server");
	}

	void ConfigManager::PrepareMacros()
	{
		_macros.clear();
		_macros["${ome.AppHome}"] = ov::PathManager::GetAppPath();
		_macros["${ome.CurrentPath}"] = ov::PathManager::GetCurrentPath();
	}

	bool ConfigManager::LoadLoggerConfig() noexcept
	{
		struct stat value = { 0 };

		ov::String config_path = ov::PathManager::Combine(ov::PathManager::GetCurrentPath("conf"), "Logger.xml");

		::memset(&_last_modified, 0, sizeof(_last_modified));
		::stat(config_path, &value);

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

		auto logger_loader = std::make_shared<ConfigLoggerLoader>(config_path);
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
}
