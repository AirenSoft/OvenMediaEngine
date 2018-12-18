//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_stream_publisher.h"
#include "config/config_manager.h"
#include "segment_stream_application.h"

#define OV_LOG_TAG "SegmentStream"

std::shared_ptr<SegmentStreamPublisher> SegmentStreamPublisher::Create(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router)
{
	auto segment_stream = std::make_shared<SegmentStreamPublisher>(application_info, router);

	// CONFIG을 불러온다.
	segment_stream->Start();

	return segment_stream;
}

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
SegmentStreamPublisher::SegmentStreamPublisher(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router) 
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
	if(dash_publisher_info == nullptr)
	{
		logte("Cannot initialize SegmentStreamPublisher(DASH) using config information");
		return false;
	}

	// Find Hls publisher configuration
    _publisher_type = cfg::PublisherType::Hls;
    auto hls_publisher_info = FindPublisherInfo<cfg::HlsPublisher>();
	if(hls_publisher_info == nullptr)
	{
		logte("Cannot initialize SegmentStreamPublisher(HLS) using config information");
		return false;
	}

    _publisher_type = publisher_type;

	uint16_t dash_publisher_port 	= dash_publisher_info->GetPort();
	uint16_t hls_publisher_port 	= hls_publisher_info->GetPort();
	ov::String app_name 			= _application_info.GetName();

	if(dash_publisher_port == 0) dash_publisher_port = 80;
	if(hls_publisher_port == 0) hls_publisher_port = 80;

	// DSH/HLS Server Start
	if(dash_publisher_port == hls_publisher_port)
	{
		auto segment_stream_server = std::make_shared<SegmentStreamServer>();

		segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::DASH);
		segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::HLS);
		segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());
		segment_stream_server->Start(ov::SocketAddress(dash_publisher_port));

		_segment_stream_servers.push_back(segment_stream_server);
	}
	else
	{
		auto dash_segment_stream_server = std::make_shared<SegmentStreamServer>();

		dash_segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::DASH);
		dash_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());
		dash_segment_stream_server->Start(ov::SocketAddress(dash_publisher_port));

		_segment_stream_servers.push_back(dash_segment_stream_server);

		auto hls_segment_stream_server = std::make_shared<SegmentStreamServer>();

		hls_segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::HLS);
		hls_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());
		hls_segment_stream_server->Start(ov::SocketAddress(hls_publisher_port));

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
bool SegmentStreamPublisher::OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name, const ov::String &file_name, PlayListType play_list_type, ov::String &play_list)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if(!stream)
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
bool SegmentStreamPublisher::OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name, SegmentType segmnet_type, const ov::String &file_name, std::shared_ptr<ov::Data> &segment_data)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if (!stream)
	{
		logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return stream->GetSegment(segmnet_type, file_name, segment_data);
}

//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnCrossdomainRequest(ov::String &cross_domain)
{
	cross_domain =  "<?xml version=\"1.0\"?>\r\n"\
					"<cross-domain-policy>\r\n"\
					"<allow-access-from domain=\"*\"/>\r\n"\
					"<site-control permitted-cross-domain-policies=\"all\"/>\r\n"\
					"</cross-domain-policy>";
	return true;
}

//====================================================================================================
// OnCorsCheck
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool SegmentStreamPublisher::OnCorsCheck(const ov::String &app_name, const ov::String &stream_name, const ov::String &file_name, ov::String &origin_url)
{

	return true;
}

