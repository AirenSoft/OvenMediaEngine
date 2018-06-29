//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_encode_loader.h"
#include "config_private.h"

ConfigEncodeLoader::ConfigEncodeLoader()
    : ConfigLoader()
{
}

ConfigEncodeLoader::ConfigEncodeLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigEncodeLoader::~ConfigEncodeLoader()
{
}

bool ConfigEncodeLoader::Parse()
{
    // validates
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node encodes_node = _document.child("Encodes");
    if(encodes_node.empty())
    {
        logte("Could not found <Encodes> config...");
        return false;
    }

    pugi::xml_node encode_node = encodes_node.child("Encode");
    if(encode_node.empty())
    {
        logte("Could not found <Encode> config...");
        return false;
    }

    // Parse Encodes
    for(encode_node; encode_node; encode_node = encode_node.next_sibling())
    {
        _encode_infos.push_back(ParseEncode(encode_node));
    }

    return true;
}

void ConfigEncodeLoader::Reset()
{
    _encode_infos.clear();

    ConfigLoader::Reset();
}

std::vector<std::shared_ptr<TranscodeEncodeInfo>> ConfigEncodeLoader::GetEncodeInfos() const noexcept
{
    return _encode_infos;
}

std::shared_ptr<TranscodeEncodeInfo> ConfigEncodeLoader::ParseEncode(pugi::xml_node encode_node)
{
    std::shared_ptr<TranscodeEncodeInfo> encode_info = std::make_shared<TranscodeEncodeInfo>();

    encode_info->SetActive(ConfigUtility::BoolFromNode(encode_node.child("Active")));
    encode_info->SetName(ConfigUtility::StringFromNode(encode_node.child("Name")));
    encode_info->SetStreamName(ConfigUtility::StringFromNode(encode_node.child("StreamName")));
    encode_info->SetContainer(ConfigUtility::StringFromNode(encode_node.child("Container")));

    pugi::xml_node video_node = encode_node.child("Video");

    if(!video_node.empty())
    {
        encode_info->SetVideo(ParseVideo(video_node));
    }

    pugi::xml_node audio_node = encode_node.child("Audio");

    if(!audio_node.empty())
    {
        encode_info->SetAudio(ParseAudio(audio_node));
    }

    return encode_info;
}

std::shared_ptr<TranscodeVideoInfo> ConfigEncodeLoader::ParseVideo(pugi::xml_node video_node)
{
    std::shared_ptr<TranscodeVideoInfo> video_info = std::make_shared<TranscodeVideoInfo>();

    video_info->SetActive(ConfigUtility::BoolFromNode(video_node.child("Active")));
    video_info->SetCodec(ConfigUtility::StringFromNode(video_node.child("Codec")));
    video_info->SetH264Profile(ConfigUtility::StringFromNode(video_node.child("H264Profile")));
    video_info->SetH264Level(ConfigUtility::StringFromNode(video_node.child("H264Level")));
    video_info->SetHWAcceleration(ConfigUtility::StringFromNode(video_node.child("HWAcceleration")));
    video_info->SetScale(ConfigUtility::StringFromNode(video_node.child("Scale")));
    video_info->SetWidth(ConfigUtility::IntFromNode(video_node.child("Width")));
    video_info->SetHeight(ConfigUtility::IntFromNode(video_node.child("Height")));
    video_info->SetBitrate(ConfigUtility::IntFromNode(video_node.child("Bitrate")));
    video_info->SetFramerate(ConfigUtility::FloatFromNode(video_node.child("Framerate")));

    return video_info;
}

std::shared_ptr<TranscodeAudioInfo> ConfigEncodeLoader::ParseAudio(pugi::xml_node audio_node)
{
    std::shared_ptr<TranscodeAudioInfo> audio_info = std::make_shared<TranscodeAudioInfo>();

    audio_info->SetActive(ConfigUtility::BoolFromNode(audio_node.child("Active")));
    audio_info->SetCodec(ConfigUtility::StringFromNode(audio_node.child("Codec")));
    audio_info->SetBitrate(ConfigUtility::IntFromNode(audio_node.child("Bitrate")));
    audio_info->SetSamplerate(ConfigUtility::IntFromNode(audio_node.child("Samplerate")));
    audio_info->SetChannel(ConfigUtility::IntFromNode(audio_node.child("Channel")));

    return audio_info;
}