//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_provider_loader.h"
#include "config_private.h"

#include "config_decode_loader.h"

ConfigProviderLoader::ConfigProviderLoader()
{
}

ConfigProviderLoader::ConfigProviderLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigProviderLoader::~ConfigProviderLoader()
{
}

bool ConfigProviderLoader::Parse()
{
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node publisher_node = _document.child("Provider");

    return Parse(publisher_node);
}

bool ConfigProviderLoader::Parse(pugi::xml_node root_node)
{
    // validates
    if(root_node.empty())
    {
        logte("Could not found <Provider> config...");
        return false;
    }

    pugi::xml_node decode_ref_node = root_node.child("Decode-Ref");
    if(decode_ref_node.empty() || decode_ref_node.text().empty())
    {
        logte("Could not found <Decode-Ref> config...");
        return false;
    }

    // Parse Provider
    _provider_info = std::make_shared<ProviderInfo>();

    _provider_info->SetTypeFromString(ConfigUtility::StringFromNode(root_node.child("Type")));

    // Parse Encodes
    ov::String decode_ref = ConfigUtility::StringFromNode(decode_ref_node);
    // TODO: MACRO 기능 정의&개발 후 수정해야함
    decode_ref = decode_ref.Replace(CONFIG_MACRO_APP_HOME, ov::PathManager::GetAppPath());

    _provider_info->SetDecodeRef(decode_ref);

    std::unique_ptr<ConfigDecodeLoader> decode_loader = std::make_unique<ConfigDecodeLoader>(decode_ref);

    if(decode_loader->Parse())
    {
        _provider_info->SetDecode(decode_loader->GetDecodeInfo());

        decode_loader->Reset();
    }

    return true;
}

void ConfigProviderLoader::Reset()
{
    _provider_info.reset();

    ConfigLoader::Reset();
}

std::shared_ptr<ProviderInfo> ConfigProviderLoader::GetProviderInfo() const noexcept
{
    return _provider_info;
}