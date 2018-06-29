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

#include <base/application/application_info.h>

class ConfigApplicationLoader : public ConfigLoader
{
public:
    ConfigApplicationLoader();
    explicit ConfigApplicationLoader(const ov::String config_path);
    virtual ~ConfigApplicationLoader();

    virtual bool Parse() override;
    void Reset();

    std::vector<std::shared_ptr<ApplicationInfo>> GetApplicationInfos() const noexcept;

private:
    std::shared_ptr<ApplicationInfo> ParseApplication(pugi::xml_node application_node);

    std::vector<std::shared_ptr<ApplicationInfo>> _application_infos;
};
