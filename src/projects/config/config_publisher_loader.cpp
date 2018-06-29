//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_publisher_loader.h"
#include "config_private.h"

#include "config_encode_loader.h"

ConfigPublisherLoader::ConfigPublisherLoader()
{
}

ConfigPublisherLoader::ConfigPublisherLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigPublisherLoader::~ConfigPublisherLoader()
{
}

bool ConfigPublisherLoader::Parse()
{
    bool result = ConfigLoader::Load();

    if(!result)
    {
        return false;
    }

    pugi::xml_node publisher_node = _document.child("Publisher");

    return Parse(publisher_node);
}

bool ConfigPublisherLoader::Parse(pugi::xml_node root_node)
{
    // validates
    if(root_node.empty())
    {
        logte("Could not found <Publisher> config...");
        return false;
    }

    pugi::xml_node encodes_ref_node = root_node.child("Encodes-Ref");
    if(encodes_ref_node.empty() || encodes_ref_node.text().empty())
    {
        logte("Could not found <Encodes-Ref> config...");
        return false;
    }

    // Parse Publisher
    _publisher_info = std::make_shared<PublisherInfo>();

    _publisher_info->SetTypeFromString(ConfigUtility::StringFromNode(root_node.child("Type")));

    // Parse Encodes
    ov::String encodes_ref = ConfigUtility::StringFromNode(encodes_ref_node);
    // TODO: MACRO 기능 정의&개발 후 수정해야함
    encodes_ref = encodes_ref.Replace(CONFIG_MACRO_APP_HOME, ov::PathManager::GetAppPath());

    _publisher_info->SetEncodesRef(encodes_ref);

    std::unique_ptr<ConfigEncodeLoader> encode_loader = std::make_unique<ConfigEncodeLoader>(encodes_ref);

    if(encode_loader->Parse())
    {
        for(const auto &encode_info : encode_loader->GetEncodeInfos())
        {
            _publisher_info->AddEncode(encode_info);
        }

        encode_loader->Reset();
    }

    return true;
}

void ConfigPublisherLoader::Reset()
{
    _publisher_info.reset();

    ConfigLoader::Reset();
}

std::shared_ptr<PublisherInfo> ConfigPublisherLoader::GetPublisherInfo() const noexcept
{
    return _publisher_info;
}