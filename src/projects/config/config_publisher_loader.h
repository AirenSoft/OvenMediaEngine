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

#include <base/application/publisher_info.h>

class ConfigPublisherLoader : public ConfigLoader
{
public:
    ConfigPublisherLoader();
    explicit ConfigPublisherLoader(const ov::String config_path);
    virtual ~ConfigPublisherLoader();

    virtual bool Parse() override;
    bool Parse(pugi::xml_node root_node);
    void Reset();

    std::shared_ptr<PublisherInfo> GetPublisherInfo() const noexcept;

private:
    std::shared_ptr<PublisherInfo> _publisher_info;
};
