//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_server_loader.h"
#include "config_private.h"

#include "config_host_loader.h"

ConfigServerLoader::ConfigServerLoader()
{
}

ConfigServerLoader::ConfigServerLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigServerLoader::~ConfigServerLoader()
{
}

bool ConfigServerLoader::Parse()
{
    // validates
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node server_node = _document.child("Server");
    if(server_node.empty())
    {
        logte("Could not found <Server> config...");
        return false;
    }

    pugi::xml_node hosts_node = server_node.child("Hosts");
    if(hosts_node.empty())
    {
        logte("Could not found <Hosts> config...");
        return false;
    }

    pugi::xml_node host_node = hosts_node.child("Host");
    if(host_node.empty())
    {
        logte("Could not found <Host> config...");
        return false;
    }

    // Parse Server
    _server_info = std::make_shared<ServerInfo>();

    _server_info->SetName(ConfigUtility::StringFromNode(server_node.child("Name")));

    // Parse Hosts
    std::unique_ptr<ConfigHostLoader> host_loader = std::make_unique<ConfigHostLoader>();
    for(host_node; host_node; host_node = host_node.next_sibling())
    {
        host_loader->Reset();
        if(host_loader->Parse(host_node))
        {
            _server_info->AddHost(host_loader->GetHostInfo());
        }
    }

    host_loader->Reset();

    return true;
}

void ConfigServerLoader::Reset()
{
    _server_info.reset();

    ConfigLoader::Reset();
}

std::shared_ptr<ServerInfo> ConfigServerLoader::GetServerInfo() const noexcept
{
    return _server_info;
}