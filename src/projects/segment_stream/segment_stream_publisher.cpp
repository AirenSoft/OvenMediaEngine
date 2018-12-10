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
// GetSegmentStreamPort
// - Config 정보에서 Segment Stream Port(http)정보 전달
//====================================================================================================
uint16_t SegmentStreamPublisher::GetSegmentStreamPort()
{
	int port = _publisher_info->GetPort();

	return static_cast<uint16_t>((port == 0) ? 80 : port);
}

//====================================================================================================
// Start
// - Publisher override
//====================================================================================================
bool SegmentStreamPublisher::Start()
{
	// Find SegmentStream publisher configuration
	_publisher_info = FindPublisherInfo<cfg::DashPublisher>();

	if(_publisher_info == nullptr)
	{
		logte("Cannot initialize DashPublisher using config information");
		return false;
	}

	// segment setream server(http) Start
	ov::String app_name = _application_info.GetName();
	_segment_stream_server = std::make_shared<SegmentStreamServer>();
	_segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::DASH); // TODO : 차후 허용 확인 이후 설정
	_segment_stream_server->SetAllowApp(app_name, AllowProtocolFlag::HLS);  // TODO : 차후 허용 확인 이후 설정
	_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());
	_segment_stream_server->Start(ov::SocketAddress(GetSegmentStreamPort()));

	// Publisher::Start()에서 Application을 생성



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
bool SegmentStreamPublisher::OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name, PlayListType play_list_type, ov::String &play_list)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
		logte("Cannot find stream (%s/%s)", app_name.CStr(), stream_name.CStr());
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

	if (!stream) {
		logte("Cannot find stream (%s/%s)", app_name.CStr(), stream_name.CStr());
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
bool SegmentStreamPublisher::OnCorsCheck(const ov::String &app_name, const ov::String &stream_name, ov::String &origin_url)
{

	return true;
}

