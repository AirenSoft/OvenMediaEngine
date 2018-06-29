//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_application_loader.h"
#include "config_private.h"

#include "config_provider_loader.h"
#include "config_publisher_loader.h"

ConfigApplicationLoader::ConfigApplicationLoader()
    :ConfigLoader()
{
}

ConfigApplicationLoader::ConfigApplicationLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigApplicationLoader::~ConfigApplicationLoader()
{
}

bool ConfigApplicationLoader::Parse()
{
    // validates
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node applications_node = _document.child("Applications");
    if(applications_node.empty())
    {
        logte("Could not found <Applications> config...");
        return false;
    }

    pugi::xml_node application_node = applications_node.child("Application");
    if(application_node.empty())
    {
        logte("Could not found <Application> config...");
        return false;
    }

    // Parse Applications
    for(application_node; application_node; application_node = application_node.next_sibling())
    {
        const auto &application = ParseApplication(application_node);

        if(application != nullptr)
        {
            _application_infos.push_back(application);
        }
        else
        {
            logtw("Failed to parse <Application>...");
        }
    }

    if(_application_infos.size() == 0)
    {
        logte("Could not found <Application> config...");
        return false;
    }

    return true;
}

void ConfigApplicationLoader::Reset()
{
    _application_infos.clear();

    ConfigLoader::Reset();
}

std::vector<std::shared_ptr<ApplicationInfo>> ConfigApplicationLoader::GetApplicationInfos() const noexcept
{
    return _application_infos;
}

std::shared_ptr<ApplicationInfo> ConfigApplicationLoader::ParseApplication(pugi::xml_node application_node)
{
    std::shared_ptr<ApplicationInfo> application_info = std::make_shared<ApplicationInfo>();

    // Parse Application

    application_info->SetName(ConfigUtility::StringFromNode(application_node.child("Name")));
    application_info->SetTypeFromString(ConfigUtility::StringFromNode(application_node.child("Type")));

    // Parse Providers
    pugi::xml_node providers_node = application_node.child("Providers");
    if(!providers_node.empty())
    {
        std::unique_ptr<ConfigProviderLoader> provider_loader = std::make_unique<ConfigProviderLoader>();
        for(pugi::xml_node provider_node = providers_node.child("Provider"); provider_node; provider_node = provider_node.next_sibling())
        {
            provider_loader->Reset();
            if(provider_loader->Parse(provider_node))
            {
                application_info->AddProvider(provider_loader->GetProviderInfo());
            }
        }
    }

    // Parse Publishers
    pugi::xml_node publishers_node = application_node.child("Publishers");
    if(!publishers_node.empty())
    {
        std::unique_ptr<ConfigPublisherLoader> publisher_loader = std::make_unique<ConfigPublisherLoader>();
        for(pugi::xml_node publisher_node = publishers_node.child("Publisher"); publisher_node; publisher_node = publisher_node.next_sibling())
        {
            publisher_loader->Reset();
            if(publisher_loader->Parse(publisher_node))
            {
                application_info->AddPublisher(publisher_loader->GetPublisherInfo());
            }
        }

        publisher_loader->Reset();
    }

    if(application_info->GetProviders().size() == 0)
    {
        logtw("Could not found <Providers> config...");
    }

    if(application_info->GetPublishers().size() == 0)
    {
        logtw("Could not found <Publishers> config...");
    }

    return application_info;
}