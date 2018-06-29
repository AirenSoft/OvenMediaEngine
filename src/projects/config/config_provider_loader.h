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

#include <base/application/provider_info.h>

class ConfigProviderLoader : public ConfigLoader
{
public:
    ConfigProviderLoader();
    explicit ConfigProviderLoader(const ov::String config_path);
    virtual ~ConfigProviderLoader();

    virtual bool Parse() override;
    bool Parse(pugi::xml_node root_node);
    void Reset();

    std::shared_ptr<ProviderInfo> GetProviderInfo() const noexcept;

private:
    std::shared_ptr<ProviderInfo> _provider_info;
};
