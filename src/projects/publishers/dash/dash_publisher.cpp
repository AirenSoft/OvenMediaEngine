//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_publisher.h"
#include "dash_application.h"
#include "dash_private.h"
#include "dash_stream_server.h"

#include <config/config_manager.h>

std::shared_ptr<DashPublisher> DashPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
													 const info::Application *application_info,
													 std::shared_ptr<MediaRouteInterface> router)
{
	return SegmentPublisher::Create<DashPublisher>(http_server_manager, application_info, std::move(router));
}

DashPublisher::DashPublisher(PrivateToken token, const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: SegmentPublisher(application_info, std::move(router))
{
}

bool DashPublisher::StartInternal(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager, int port,
								  const std::shared_ptr<SegmentStreamServer> &stream_server, const std::vector<cfg::Url> &cross_domains,
								  int segment_count, int segment_duration, int thread_count)
{
	if (_application_info->GetOrigin().IsParsed())
	{
		// OME is running as edge
	}
	else
	{
		if (CheckCodecAvailability({"h264"}, {"aac"}) == false)
		{
			return false;
		}
	}

	auto host = _application_info->GetParentAs<cfg::Host>("Host");
	ov::SocketAddress address(host->GetIp(), port);

	// Register as observer
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	stream_server->SetCrossDomain(cross_domains);

	// Start the DASH Server
	if (stream_server->Start(
			address, http_server_manager,
			_application_info->GetName(),
			thread_count,
			_application_info->GetCertificate(), _application_info->GetChainCertificate()) == false)
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	logtd("%s Publisher is created successfully on %s (SegmentCount: %d, SegmentDuration: %d, Threads: %d)",
		  GetPublisherName(), address.ToString().CStr(), segment_count, segment_duration, thread_count);

	return Publisher::Start();
}

bool DashPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto host = _application_info->GetParentAs<cfg::Host>("Host");
	auto port = host->GetPorts().GetDashPort();
	auto publisher_info = _application_info->GetPublisher<cfg::DashPublisher>();

	return StartInternal(http_server_manager, port.GetPort(), std::make_shared<DashStreamServer>(),
						 publisher_info->GetCrossDomains(),
						 publisher_info->GetSegmentCount(), publisher_info->GetSegmentDuration(),
						 publisher_info->GetThreadCount());
}

std::shared_ptr<Application> DashPublisher::OnCreateApplication(const info::Application &application_info)
{
	return DashApplication::Create(application_info);
}
