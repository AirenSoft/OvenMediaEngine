//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_stream_publisher.h"
#include "../config/config_manager.h"
#include "segment_stream_application.h"

#define OV_LOG_TAG "SegmentStream"

std::shared_ptr<SegmentStreamPublisher>
SegmentStreamPublisher::Create(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router)
{
    auto segment_stream = std::make_shared<SegmentStreamPublisher>(application_info, router);

    // CONFIG을 불러온다.
    segment_stream->Start();

    return segment_stream;
}

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
SegmentStreamPublisher::SegmentStreamPublisher(const info::Application &application_info,
                                               std::shared_ptr<MediaRouteInterface> router)
        : Publisher(application_info, std::move(router))
{
    _publisher_type = cfg::PublisherType::Dash;


    logtd("SegmentStreamPublisher Create Start");
}

//====================================================================================================
// ~SegmentStreamPublisher
//====================================================================================================
SegmentStreamPublisher::~SegmentStreamPublisher()
{
    logtd("SegmentStreamPublisher has been terminated finally");
}

//====================================================================================================
// Start
// - Publisher override
//====================================================================================================
bool SegmentStreamPublisher::Start()
{

    auto publisher_type = GetPublisherType();

    // Find Dash publisher configuration
    _publisher_type = cfg::PublisherType::Dash;
    auto dash_publisher_info = FindPublisherInfo<cfg::DashPublisher>();
    if (dash_publisher_info == nullptr)
    {
        logte("Cannot initialize SegmentStreamPublisher(DASH) using config information");
        return false;
    }

    // Find Hls publisher configuration
    _publisher_type = cfg::PublisherType::Hls;
    auto hls_publisher_info = FindPublisherInfo<cfg::HlsPublisher>();
    if (hls_publisher_info == nullptr)
    {
        logte("Cannot initialize SegmentStreamPublisher(HLS) using config information");
        return false;
    }

    _publisher_type = publisher_type;

    // DSH/HLS Server Start
    if (dash_publisher_info->GetPort() == hls_publisher_info->GetPort())
    {
        auto segment_stream_server = std::make_shared<SegmentStreamServer>();

        segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::DASH);
        segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::HLS);
        segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

        // Cors/CrossDomain
        segment_stream_server->AddCors(dash_publisher_info->GetCorsUrls(), ProtocolFlag::DASH);
        segment_stream_server->AddCrossDomain(dash_publisher_info->GetCrossDomains());
        segment_stream_server->AddCors(hls_publisher_info->GetCorsUrls(), ProtocolFlag::HLS);
        segment_stream_server->AddCrossDomain(hls_publisher_info->GetCrossDomains());

        // TLS
        std::shared_ptr<Certificate> certificate = GetCertificate(dash_publisher_info->GetTls().GetCertPath(),
                                                                  dash_publisher_info->GetTls().GetKeyPath());
        if(certificate == nullptr)
        {
            certificate = GetCertificate(hls_publisher_info->GetTls().GetCertPath(),
                                         hls_publisher_info->GetTls().GetKeyPath());
        }

        // Start
        segment_stream_server->Start(ov::SocketAddress(dash_publisher_info->GetPort()), certificate);
        _segment_stream_servers.push_back(segment_stream_server);
    }
    else
    {
        auto dash_segment_stream_server = std::make_shared<SegmentStreamServer>();

        dash_segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::DASH);
        dash_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

        // Cors/CrossDomain
        dash_segment_stream_server->AddCors(dash_publisher_info->GetCorsUrls(), ProtocolFlag::DASH);
        dash_segment_stream_server->AddCrossDomain(dash_publisher_info->GetCrossDomains());

        // Start
        dash_segment_stream_server->Start(ov::SocketAddress(dash_publisher_info->GetPort()),
                                          GetCertificate(dash_publisher_info->GetTls().GetCertPath(),
                                                         dash_publisher_info->GetTls().GetKeyPath()));

        _segment_stream_servers.push_back(dash_segment_stream_server);

        auto hls_segment_stream_server = std::make_shared<SegmentStreamServer>();

        hls_segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::HLS);
        hls_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

        // Cors/CrossDomain
        hls_segment_stream_server->AddCors(hls_publisher_info->GetCorsUrls(), ProtocolFlag::HLS);
        hls_segment_stream_server->AddCrossDomain(hls_publisher_info->GetCrossDomains());

        // Start
        hls_segment_stream_server->Start(ov::SocketAddress(hls_publisher_info->GetPort()),
                                         GetCertificate(hls_publisher_info->GetTls().GetCertPath(),
                                                        hls_publisher_info->GetTls().GetKeyPath()));
        _segment_stream_servers.push_back(hls_segment_stream_server);

    }

    return Publisher::Start();
}

//====================================================================================================
// Stop
// - Publisher override
//====================================================================================================
bool SegmentStreamPublisher::Stop()
{
    return Publisher::Stop();
}


//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool SegmentStreamPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
    for(auto server : _segment_stream_servers)
        server->GetMonitoringCollectionData(collections);

    return true;
}

//====================================================================================================
// OnCreateApplication
//====================================================================================================
std::shared_ptr<Application> SegmentStreamPublisher::OnCreateApplication(const info::Application &application_info)
{
    return SegmentStreamApplication::Create(application_info);
}


//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnPlayListRequest(const ov::String &app_name,
                                               const ov::String &stream_name,
                                               const ov::String &file_name,
                                               PlayListType play_list_type,
                                               ov::String &play_list)
{
    auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

    if (!stream)
    {
        logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

    return stream->GetPlayList(play_list_type, play_list);
}

//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnSegmentRequest(const ov::String &app_name,
                                              const ov::String &stream_name,
                                              SegmentType segmnet_type,
                                              const ov::String &file_name,
                                              std::shared_ptr<ov::Data> &segment_data)
{
    auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

    if (!stream)
    {
        logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

    return stream->GetSegment(segmnet_type, file_name, segment_data);
}

std::shared_ptr<Certificate> SegmentStreamPublisher::GetCertificate(ov::String cert_path, ov::String key_path)
{
    if(!cert_path.IsEmpty() && !key_path.IsEmpty())
    {
        auto certificate = std::make_shared<Certificate>();

        logti("Trying to create a certificate using files - cert_path(%s) key_path(%s)",cert_path.CStr(),key_path.CStr());

        auto error = certificate->GenerateFromPem(cert_path, key_path);

        if(error == nullptr)
        {
            return certificate;
        }

        logte("Could not create a certificate from files: %s", error->ToString().CStr());
    }
    else
    {
        // TLS is disabled
    }

    return nullptr;
}
