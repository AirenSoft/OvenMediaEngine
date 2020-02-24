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
													 const cfg::Server &server_config,
													 const info::Host &host_info,
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<DashPublisher>(http_server_manager, server_config, host_info, router);
}

DashPublisher::DashPublisher(PrivateToken token,
							const cfg::Server &server_config,
							const info::Host &host_info,
							const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, host_info, router)
{
}

bool DashPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto server_config = GetServerConfig();
	auto host_info = GetHostInfo();
	auto ip = server_config.GetIp();
	auto port = server_config.GetBind().GetPublishers().GetDashPort();

	ov::SocketAddress address(ip, port);

	// Register as observer
	auto stream_server = std::make_shared<DashStreamServer>();
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	// TODO(Dimiden): The Cross Domain configure must be at VHost Level.
	//stream_server->SetCrossDomain(cross_domains);

	// Start the DASH Server
	if (!stream_server->Start(address, http_server_manager, DEFAULT_SEGMENT_WORKER_THREAD_COUNT,
							 host_info.GetCertificate(), host_info.GetChainCertificate()))
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	return Publisher::Start();
}

std::shared_ptr<pub::Application> DashPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if(!application_info.CheckCodecAvailability({"h264"}, {"aac"}))
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
		// return nullptr;
	}

	return DashApplication::Create(application_info);
}