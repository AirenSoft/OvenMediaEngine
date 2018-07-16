//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "config_server_loader.h"
#include "config_application_loader.h"

#include <base/ovlibrary/ovlibrary.h>

class ConfigManager : public ov::Singleton<ConfigManager>
{
public:
    friend class ov::Singleton<ConfigManager>;

    virtual ~ConfigManager();

    bool LoadConfigs();

    std::shared_ptr<ServerInfo> GetServer() const noexcept;

    std::vector<std::shared_ptr<HostInfo>> GetHosts() const noexcept;
    std::shared_ptr<HostInfo> GetHost(uint32_t id) const noexcept;

    std::shared_ptr<HostInfo> GetHost() const noexcept;

    std::shared_ptr<HostProviderInfo> GetHostProvider() const noexcept;
    std::shared_ptr<HostPublisherInfo> GetHostPublisher() const noexcept;

    std::vector<std::shared_ptr<ApplicationInfo>> GetApplicationInfos() const noexcept;
	std::shared_ptr<ApplicationInfo> GetApplicationInfo(const ov::String &name) const noexcept;

protected:
    ConfigManager();

    void LoadLoggerConfig() noexcept;
    bool LoadServerConfig() noexcept;

    std::shared_ptr<ServerInfo> _server;

	timespec _last_modified;
};
