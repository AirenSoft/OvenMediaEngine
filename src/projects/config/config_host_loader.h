//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "config_loader.h"

#include <base/application/host_info.h>

class ConfigHostLoader : public ConfigLoader
{
public:
    ConfigHostLoader();
    explicit ConfigHostLoader(const ov::String config_path);
    virtual ~ConfigHostLoader();

    virtual bool Parse() override;
    bool Parse(pugi::xml_node root_node);
    void Reset();

    std::shared_ptr<HostInfo> GetHostInfo() const noexcept;

private:
	std::shared_ptr<HostTlsInfo> ParseTls(pugi::xml_node tls_node);
    std::shared_ptr<HostProviderInfo> ParseProvider(pugi::xml_node provider_node);
    std::shared_ptr<HostPublisherInfo> ParsePublisher(pugi::xml_node publisher_node);
    void FillHostDefaultValues(std::shared_ptr<HostBaseInfo> host_base_info);

    std::shared_ptr<HostInfo> _host_info;
};
