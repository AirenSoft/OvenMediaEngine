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

#include <base/application/transcode_decode_info.h>

class ConfigDecodeLoader : public ConfigLoader
{
public:
    ConfigDecodeLoader();
    explicit ConfigDecodeLoader(const ov::String config_path);
    virtual ~ConfigDecodeLoader();

    virtual bool Parse() override;
    void Reset();

    std::shared_ptr<TranscodeDecodeInfo> GetDecodeInfo() const noexcept;

private:
    std::shared_ptr<TranscodeDecodeInfo> ParseDecode(pugi::xml_node decode_node);

    std::shared_ptr<TranscodeDecodeInfo> _decode_info;
};
