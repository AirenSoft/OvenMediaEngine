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
		_server = std::make_shared<cfg::Server>();

		return _server->Parse(config_path, "Server");
	}

//std::shared_ptr<ServerInfo> ConfigManager::GetServer() const noexcept
//{
//	return _server;
//}
//
//std::vector<std::shared_ptr<HostInfo>> ConfigManager::GetHosts() const noexcept
//{
//	if(_server == nullptr)
//	{
//		return {};
//	}
//
//	return _server->GetHosts();
//}
//
//std::shared_ptr<HostInfo> ConfigManager::GetHost(uint32_t id) const noexcept
//{
//	if(_server == nullptr)
//	{
//		return nullptr;
//	}
//
//	std::vector<std::shared_ptr<HostInfo>> hosts = _server->GetHosts();
//
//	for(const auto &host : _server->GetHosts())
//	{
//		if(host->GetId() == id)
//		{
//			return host;
//		}
//	}
//
//	return nullptr;
//}
//
//std::shared_ptr<HostInfo> ConfigManager::GetHost() const noexcept
//{
//	if(_server == nullptr)
//	{
//		return nullptr;
//	}
//
//	if(_server->GetHosts().size() == 0)
//	{
//		return nullptr;
//	}
//
//	return _server->GetHosts().front();
//}
//
//std::shared_ptr<HostTlsInfo> ConfigManager::GetHostTls() const noexcept
//{
//	const auto &host = GetHost();
//
//	if(host == nullptr)
//	{
//		return nullptr;
//	}
//
//	return host->GetTls();
//}
//
//std::shared_ptr<HostProviderInfo> ConfigManager::GetHostProvider() const noexcept
//{
//	const auto &host = GetHost();
//
//	if(host == nullptr)
//	{
//		return nullptr;
//	}
//
//	return host->GetProvider();
//}
//
//std::shared_ptr<HostPublisherInfo> ConfigManager::GetHostPublisher() const noexcept
//{
//	const auto &host = GetHost();
//
//	if(host == nullptr)
//	{
//		return nullptr;
//	}
//
//	return host->GetPublisher();
//}
//
//std::vector<std::shared_ptr<ApplicationInfo>> ConfigManager::GetApplicationInfos() const noexcept
//{
//	if(_server == nullptr)
//	{
//		return {};
//	}
//
//	if(_server->GetHosts().size() == 0)
//	{
//		return {};
//	}
//
//	return _server->GetHosts().front()->GetApplications();
//}
//
//std::shared_ptr<ApplicationInfo> ConfigManager::GetApplicationInfo(const ov::String &name) const noexcept
//{
//	auto list = GetApplicationInfos();
//
//	for(auto app_info : list)
//	{
//		if(app_info->GetName() == name)
//		{
//			return app_info;
//		}
//	}
//
//	return nullptr;
//}

	void ConfigManager::PrepareMacros()
	{
		ov::String app_path = ov::PathManager::GetAppPath();

		_macros.clear();
		_macros["${ome.AppHome}"] = app_path;
	}

	bool ConfigManager::LoadLoggerConfig() noexcept
	{
		struct stat value = { 0 };

		ov::String config_path = ov::PathManager::Combine(ov::PathManager::GetAppPath(), "conf/Logger.xml");

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
