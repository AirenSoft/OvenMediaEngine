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

	// Find Hls publisher configuration
	_publisher_type = cfg::PublisherType::Hls;
	auto hls_publisher_info = FindPublisherInfo<cfg::HlsPublisher>();

	_publisher_type = publisher_type;

	// disable both DASH and HLS
	if(!dash_publisher_info->IsParsed() && !hls_publisher_info->IsParsed())
	{
		logtd("DASH/HLS disable setting");
		return true;
	}

	auto certificate = _application_info.GetCertificate();
	auto chain_certificate = _application_info.GetChainCertificate();

	// DSH/HLS Server Start
	// same port or one disable
	if((dash_publisher_info->GetListenPort() == hls_publisher_info->GetListenPort()) ||
	   !dash_publisher_info->IsParsed() ||
	   !hls_publisher_info->IsParsed())
	{
		auto segment_stream_server = std::make_shared<SegmentStreamServer>();
		std::shared_ptr<ov::SocketAddress> address;
		int thread_count = 0;
		int max_retry_count = 0;
		int send_buffer_size = 0;
		int recv_buffer_size = 0;

		if(dash_publisher_info->IsParsed())
		{
			segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::DASH);
			segment_stream_server->AddCors(dash_publisher_info->GetCrossDomains(), ProtocolFlag::DASH);
			segment_stream_server->AddCrossDomain(dash_publisher_info->GetCrossDomains());

			address = std::make_shared<ov::SocketAddress>(dash_publisher_info->GetListenPort());
			thread_count = dash_publisher_info->GetThreadCount();
			max_retry_count = dash_publisher_info->GetSegmentDuration() * 1.5;
			send_buffer_size = dash_publisher_info->GetSendBufferSize();
			recv_buffer_size = dash_publisher_info->GetRecvBufferSize();
		}

		if(hls_publisher_info->IsParsed())
		{
			segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::HLS);
			segment_stream_server->AddCors(hls_publisher_info->GetCrossDomains(), ProtocolFlag::HLS);
			segment_stream_server->AddCrossDomain(hls_publisher_info->GetCrossDomains());

			if(!dash_publisher_info->IsParsed())
			{
				address = std::make_shared<ov::SocketAddress>(hls_publisher_info->GetListenPort());
				thread_count = hls_publisher_info->GetThreadCount();
				max_retry_count = hls_publisher_info->GetSegmentDuration() * 1.5;
				send_buffer_size = hls_publisher_info->GetSendBufferSize();
				recv_buffer_size = hls_publisher_info->GetRecvBufferSize();
			}
		}

		segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

		// DASH + HLS Server Start
		segment_stream_server->Start(*(address.get()),
		                             thread_count,
		                             max_retry_count,
		                             send_buffer_size,
		                             recv_buffer_size,
		                             certificate);

		_segment_stream_servers.push_back(segment_stream_server);
	}
	else
	{
		auto dash_segment_stream_server = std::make_shared<SegmentStreamServer>();

		dash_segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::DASH);
		dash_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

		// Cors/CrossDomain
		dash_segment_stream_server->AddCors(dash_publisher_info->GetCrossDomains(), ProtocolFlag::DASH);
		dash_segment_stream_server->AddCrossDomain(dash_publisher_info->GetCrossDomains());

		// Dash Server Start
		dash_segment_stream_server->Start(ov::SocketAddress(dash_publisher_info->GetListenPort()),
		                                  dash_publisher_info->GetThreadCount(),
		                                  dash_publisher_info->GetSegmentDuration() * 1.5,
		                                  dash_publisher_info->GetSendBufferSize(),
		                                  dash_publisher_info->GetRecvBufferSize(),
		                                  certificate, chain_certificate
		);

		_segment_stream_servers.push_back(dash_segment_stream_server);

		auto hls_segment_stream_server = std::make_shared<SegmentStreamServer>();

		hls_segment_stream_server->SetAllowApp(_application_info.GetName(), ProtocolFlag::HLS);
		hls_segment_stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

		// Cors/CrossDomain
		hls_segment_stream_server->AddCors(hls_publisher_info->GetCrossDomains(), ProtocolFlag::HLS);
		hls_segment_stream_server->AddCrossDomain(hls_publisher_info->GetCrossDomains());

		// HLS Server Start
		hls_segment_stream_server->Start(ov::SocketAddress(hls_publisher_info->GetListenPort()),
		                                 hls_publisher_info->GetThreadCount(),
		                                 hls_publisher_info->GetSegmentDuration() * 1.5,
		                                 hls_publisher_info->GetSendBufferSize(),
		                                 hls_publisher_info->GetRecvBufferSize(),
		                                 certificate, chain_certificate
		);

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
	{
		server->GetMonitoringCollectionData(collections);
	}

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
bool SegmentStreamPublisher::OnSegmentRequest(const ov::String &app_name,
                                              const ov::String &stream_name,
                                              SegmentType segmnet_type,
                                              const ov::String &file_name,
                                              std::shared_ptr<ov::Data> &segment_data)
{
	auto stream = std::static_pointer_cast<SegmentStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
		logte("Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return stream->GetSegment(segmnet_type, file_name, segment_data);
}
