//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
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

		bool LoadConfigs(ov::String config_path);
		// Load configs from default path (<binary_path>/conf/*)
		bool LoadConfigs();

		std::shared_ptr<Server> GetServer() noexcept
		{
			return _server;
		}

		//
		//std::shared_ptr<ServerInfo> GetServer() const noexcept;
		//
		//std::vector<std::shared_ptr<HostInfo>> GetHosts() const noexcept;
		//std::shared_ptr<HostInfo> GetHost(uint32_t id) const noexcept;
		//
		//std::shared_ptr<HostInfo> GetHost() const noexcept;
		//
		//std::shared_ptr<HostTlsInfo> GetHostTls() const noexcept;
		//std::shared_ptr<HostProviderInfo> GetHostProvider() const noexcept;
		//std::shared_ptr<HostPublisherInfo> GetHostPublisher() const noexcept;
		//
		//std::vector<std::shared_ptr<ApplicationInfo>> GetApplicationInfos() const noexcept;
		//std::shared_ptr<ApplicationInfo> GetApplicationInfo(const ov::String &name) const noexcept;

		ov::String ResolveMacros(ov::String string);

	protected:
		ConfigManager();

		void PrepareMacros();

		bool LoadLoggerConfig(const ov::String &config_path) noexcept;

		bool IsValidVersion(const std::string& name, const std::string& version);

		std::shared_ptr<Server> _server;
		std::map<ov::String, ov::String> _macros;

		timespec _last_modified;

        std::map<std::string, std::vector<std::string>> _supported_xml;
	};
}