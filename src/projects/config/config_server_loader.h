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

#include <base/application/server_info.h>

class ConfigServerLoader : public ConfigLoader
{
public:
    ConfigServerLoader();
    explicit ConfigServerLoader(const ov::String config_path);
    virtual ~ConfigServerLoader();

    virtual bool Parse() override;
    void Reset();

    std::shared_ptr<ServerInfo> GetServerInfo() const noexcept;

private:
    std::shared_ptr<ServerInfo> _server_info;
};
