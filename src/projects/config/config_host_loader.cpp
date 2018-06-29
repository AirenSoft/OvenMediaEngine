//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_host_loader.h"
#include "config_private.h"

#include "config_application_loader.h"

void ParseHostBase(pugi::xml_node node, std::shared_ptr<HostBaseInfo> host_base_info);
int ExtractPort(ov::String port_protocol);
ov::String ExtractProtocol(ov::String port_protocol);

ConfigHostLoader::ConfigHostLoader()
{
}

ConfigHostLoader::ConfigHostLoader(const ov::String config_path)
    : ConfigLoader(config_path)
{
}

ConfigHostLoader::~ConfigHostLoader()
{
}

bool ConfigHostLoader::Parse()
{
    bool result = ConfigLoader::Load();

    if(!result)
    {
        return false;
    }

    pugi::xml_node host_node = _document.child("Host");

    return Parse(host_node);
}

bool ConfigHostLoader::Parse(pugi::xml_node root_node)
{
    // validates
    if(root_node.empty())
    {
        logte("Could not found <Host> config...");
        return false;
    }

    pugi::xml_node providers_node = root_node.child("Provider");
    if(providers_node.empty())
    {
        logte("Could not found <Provider> config...");
        return false;
    }

    pugi::xml_node publishers_node = root_node.child("Publisher");
    if(publishers_node.empty())
    {
        logte("Could not found <Publisher> config...");
        return false;
    }

    pugi::xml_node applications_ref_node = root_node.child("Applications-Ref");
    if(applications_ref_node.empty() || applications_ref_node.text().empty())
    {
        logte("Could not found <Applications-Ref> config...");
        return false;
    }

    // Parse Host
    _host_info = std::make_shared<HostInfo>();

    _host_info->SetName(ConfigUtility::StringFromNode(root_node.child("Name")));
    ParseHostBase(root_node, _host_info);

    // Parse Providers
    _host_info->SetProvider(ParseProvider(providers_node));
    FillHostDefaultValues(_host_info->GetProvider());

    // Parse Publishers
    _host_info->SetPublisher(ParsePublisher(publishers_node));
    FillHostDefaultValues(_host_info->GetPublisher());

    // Parse Applications
    ov::String applications_ref = ConfigUtility::StringFromNode(applications_ref_node);
    // TODO: MACRO 기능 정의&개발 후 수정해야함
    applications_ref = applications_ref.Replace(CONFIG_MACRO_APP_HOME, ov::PathManager::GetAppPath());

    _host_info->SetApplicationsRef(applications_ref);

    std::unique_ptr<ConfigApplicationLoader> application_loader = std::make_unique<ConfigApplicationLoader>(applications_ref);

    if(application_loader->Parse())
    {
        for(const auto &application_info : application_loader->GetApplicationInfos())
        {
            _host_info->AddApplication(application_info);
        }

        application_loader->Reset();
    }

    return true;
}

void ConfigHostLoader::Reset()
{
    _host_info.reset();

    ConfigLoader::Reset();
}

std::shared_ptr<HostInfo> ConfigHostLoader::GetHostInfo() const noexcept
{
    return _host_info;
}

std::shared_ptr<HostProviderInfo> ConfigHostLoader::ParseProvider(pugi::xml_node provider_node)
{
    std::shared_ptr<HostProviderInfo> provider_info = std::make_shared<HostProviderInfo>();

    ParseHostBase(provider_node, provider_info);

    return provider_info;
}

std::shared_ptr<HostPublisherInfo> ConfigHostLoader::ParsePublisher(pugi::xml_node publisher_node)
{
    std::shared_ptr<HostPublisherInfo> publisher_info = std::make_shared<HostPublisherInfo>();

    ParseHostBase(publisher_node, publisher_info);

    // Parse WebRTC Properties
    pugi::xml_node webrtc_node = publisher_node.child("WebRTC");
    if(!webrtc_node.empty())
    {
        std::shared_ptr<HostPublishersWebRtcInfo> webrtc_properties = std::make_shared<HostPublishersWebRtcInfo>();

        webrtc_properties->SetSessionTimeout(ConfigUtility::IntFromNode(webrtc_node.child("SessionTimeout")));

        ov::String port_protocol = ConfigUtility::StringFromNode(webrtc_node.child("CandidatePort"));
        if(!port_protocol.IsEmpty())
        {
            webrtc_properties->SetCandidatePort(ExtractPort(port_protocol));
            webrtc_properties->SetCandidateProtocol(ExtractProtocol(port_protocol));
        }

        port_protocol = ConfigUtility::StringFromNode(webrtc_node.child("SignallingPort"));
        if(!port_protocol.IsEmpty())
        {
            webrtc_properties->SetSignallingPort(ExtractPort(port_protocol));
            webrtc_properties->SetSignallingProtocol(ExtractProtocol(port_protocol));
        }

        publisher_info->SetWebRtcProperties(webrtc_properties);
    }

    return publisher_info;
}

void ConfigHostLoader::FillHostDefaultValues(std::shared_ptr<HostBaseInfo> host_base_info)
{
    // 설정되지 않은 값들을, 상위(Host) 객체의 기본값으로 설정
    if(host_base_info != nullptr)
    {
        if(host_base_info->GetMaxConnection() == -1)
        {
            host_base_info->SetMaxConnection(_host_info->GetMaxConnection());
        }

        if(host_base_info->GetIPAddress().IsEmpty())
        {
            host_base_info->SetIPAddress(_host_info->GetIPAddress());
        }
    }
}

void ParseHostBase(pugi::xml_node node, std::shared_ptr<HostBaseInfo> host_base_info)
{
    host_base_info->SetIPAddress(ConfigUtility::StringFromNode(node.child("IPAddress")));
    host_base_info->SetMaxConnection(ConfigUtility::IntFromNode(node.child("MaxConnection")));

    ov::String port_protocol = ConfigUtility::StringFromNode(node.child("Port"));
    if(!port_protocol.IsEmpty())
    {
        host_base_info->SetPort(ExtractPort(port_protocol));
        host_base_info->SetProtocol(ExtractProtocol(port_protocol));
    }
}

int ExtractPort(ov::String port_protocol)
{
    std::vector<ov::String> split_port_protocol = port_protocol.Split("/");

    return ov::Converter::ToInt32(split_port_protocol[0].CStr());
}

ov::String ExtractProtocol(ov::String port_protocol)
{
    std::vector<ov::String> split_port_protocol = port_protocol.Split("/");

    if(split_port_protocol.size() > 1)
    {
        return split_port_protocol[1];
    }
    else
    {
        return "tcp";
    }
}