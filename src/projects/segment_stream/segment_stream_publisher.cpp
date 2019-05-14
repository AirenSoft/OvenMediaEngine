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
#include "segment_stream_private.h"

std::shared_ptr<SegmentStreamPublisher>
SegmentStreamPublisher::Create(cfg::PublisherType publisher_type,
                                std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                                const info::Application *application_info,
                                std::shared_ptr<MediaRouteInterface> router)
{
	auto segment_stream = std::make_shared<SegmentStreamPublisher>(publisher_type, application_info, router);

	segment_stream->Start(http_server_manager);

	return segment_stream;
}

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
SegmentStreamPublisher::SegmentStreamPublisher(cfg::PublisherType publisher_type,
                                                const info::Application *application_info,
                                                std::shared_ptr<MediaRouteInterface> router)
	: Publisher(application_info, std::move(router))
{
	_publisher_type = publisher_type;
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
bool SegmentStreamPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto publisher_type = GetPublisherType();

	if(GetPublisherType() == cfg::PublisherType::Dash)
	    return DashStart(http_server_manager);
    else if (GetPublisherType() == cfg::PublisherType::Hls)
        return HlsStart(http_server_manager);

    return true;
}

//====================================================================================================
// Dash Stream Server Start
//====================================================================================================
bool SegmentStreamPublisher::DashStart(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
    // Find Dash publisher configuration
    auto publisher_info = _application_info->GetPublisher<cfg::DashPublisher>();

    auto stream_server = std::make_shared<DashStreamServer>();

    auto certificate = _application_info->GetCertificate();

    auto chain_certificate = _application_info->GetChainCertificate();

    auto host = _application_info->GetParentAs<cfg::Host>("Host");

    if(host == nullptr)
    {
        OV_ASSERT2(false);
        logte("Invalid configuration");
        return false;
    }

    auto ports = host->GetPorts();

    const auto &port = ports.GetDashPort();

    // CORS/Crossdomain.xml setting
    stream_server->SetCrossDomain(publisher_info->GetCrossDomains());

    stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

    // DASH Server Start
    stream_server->Start(ov::SocketAddress(port.GetPort()),
                         http_server_manager,
                        _application_info->GetName(),
                        publisher_info->GetThreadCount(),
                        publisher_info->GetSegmentDuration() * 1.5,
                        publisher_info->GetSendBufferSize(),
                        publisher_info->GetRecvBufferSize(),
                        certificate,
                        chain_certificate);

    _segment_stream_server = stream_server;

    logtd("DASH Publisher Create Start");

    return Publisher::Start();
}

//====================================================================================================
// HLS Stream Server Start
//====================================================================================================
bool SegmentStreamPublisher::HlsStart(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
    // Find Hls publisher configuration
    auto publisher_info = _application_info->GetPublisher<cfg::HlsPublisher>();

    auto stream_server = std::make_shared<HlsStreamServer>();

    auto certificate = _application_info->GetCertificate();

    auto chain_certificate = _application_info->GetChainCertificate();

    auto host = _application_info->GetParentAs<cfg::Host>("Host");

    if(host == nullptr)
    {
        OV_ASSERT2(false);
        logte("Invalid configuration");
        return false;
    }

    auto ports = host->GetPorts();

    const auto &port = ports.GetHlsPort();

    // CORS/Crossdomain.xml setting
    stream_server->SetCrossDomain(publisher_info->GetCrossDomains());

    stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

    // DASH Server Start
    stream_server->Start(ov::SocketAddress(port.GetPort()),
                         http_server_manager,
                         _application_info->GetName(),
                         publisher_info->GetThreadCount(),
                         publisher_info->GetSegmentDuration() * 1.5,
                         publisher_info->GetSendBufferSize(),
                         publisher_info->GetRecvBufferSize(),
                         certificate,
                         chain_certificate);

    _segment_stream_server = stream_server;

    logtd("HLS Publisher Create Start");

    return Publisher::Start();
}

//====================================================================================================
// Stop
// - Publisher override
//======================== ============================================================================
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
    if(_segment_stream_server == nullptr)
        return false;

    return _segment_stream_server->GetMonitoringCollectionData(collections);
}

//====================================================================================================
// OnCreateApplication
//====================================================================================================
std::shared_ptr<Application> SegmentStreamPublisher::OnCreateApplication(const info::Application *application_info)
{
	return SegmentStreamApplication::Create(_publisher_type, application_info);
}

//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnPlayListRequest(const ov::String &app_name,
                                               const ov::String &stream_name,
                                               const ov::String &file_name,
                                               ov::String &play_list)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
		logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return stream->GetPlayList(play_list);
}

//====================================================================================================
// OnSegmentRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnSegmentRequest(const ov::String &app_name,
                                              const ov::String &stream_name,
                                              const ov::String &file_name,
                                              std::shared_ptr<ov::Data> &segment_data)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
		logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return stream->GetSegment(file_name, segment_data);
}
