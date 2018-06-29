//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_decode_loader.h"
#include "config_private.h"

ConfigDecodeLoader::ConfigDecodeLoader()
    : ConfigLoader()
{
}

ConfigDecodeLoader::ConfigDecodeLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{

}

ConfigDecodeLoader::~ConfigDecodeLoader()
{
}

bool ConfigDecodeLoader::Parse()
{
    // validates
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node decode_node = _document.child("Decode");
    if(decode_node.empty())
    {
        logte("Could not found <Decode> config...");
        return false;
    }

    // Parse Decode
    _decode_info = ParseDecode(decode_node);

    return true;
}

void ConfigDecodeLoader::Reset()
{
    _decode_info.reset();

    ConfigLoader::Reset();
}

std::shared_ptr<TranscodeDecodeInfo> ConfigDecodeLoader::GetDecodeInfo() const noexcept
{
    return _decode_info;
}

std::shared_ptr<TranscodeDecodeInfo> ConfigDecodeLoader::ParseDecode(pugi::xml_node decode_node)
{
    pugi::xml_node video_node = decode_node.child("Video");

    if(video_node.empty())
    {
        return nullptr;
    }

    std::shared_ptr<TranscodeDecodeInfo> decode_info = std::make_shared<TranscodeDecodeInfo>();

    std::shared_ptr<TranscodeVideoInfo> video_info = std::make_shared<TranscodeVideoInfo>();

    video_info->SetHWAcceleration(ConfigUtility::StringFromNode(video_node.child("HWAcceleration")));

    decode_info->SetVideo(video_info);

    return decode_info;
}