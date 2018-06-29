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

#include <base/application/transcode_encode_info.h>

class ConfigEncodeLoader : public ConfigLoader
{
public:
    ConfigEncodeLoader();
    explicit ConfigEncodeLoader(const ov::String config_path);
    virtual ~ConfigEncodeLoader();

    virtual bool Parse() override;
    void Reset();

    std::vector<std::shared_ptr<TranscodeEncodeInfo>> GetEncodeInfos() const noexcept;

private:
    std::shared_ptr<TranscodeEncodeInfo> ParseEncode(pugi::xml_node encode_node);
    std::shared_ptr<TranscodeVideoInfo> ParseVideo(pugi::xml_node video_node);
    std::shared_ptr<TranscodeAudioInfo> ParseAudio(pugi::xml_node audio_node);

    std::vector<std::shared_ptr<TranscodeEncodeInfo>> _encode_infos;
};
