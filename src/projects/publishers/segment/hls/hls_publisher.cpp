//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_publisher.h"
#include "hls_application.h"
#include "hls_private.h"
#include "hls_stream_server.h"

#include <config/config_manager.h>
#include <modules/signed_url/signed_url.h>
#include <orchestrator/orchestrator.h>

std::shared_ptr<HlsPublisher> HlsPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
												   const cfg::Server &server_config,
												   const info::Host &host_info,
												   const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<HlsPublisher>(http_server_manager, server_config, host_info, router);
}

HlsPublisher::HlsPublisher(PrivateToken token,
						   const cfg::Server &server_config,
						   const info::Host &host_info,
						   const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, host_info, router)
{
}

bool HlsPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto &server_config = GetServerConfig();
	auto &host_info = GetHostInfo();

	//auto &name = host_info.GetName();

	auto &ip = server_config.GetIp();
	auto port = server_config.GetBind().GetPublishers().GetDashPort();

	ov::SocketAddress address(ip, port);

	// Register as observer
	auto stream_server = std::make_shared<HlsStreamServer>();
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	// TODO(Dimiden): The Cross Domain configure must be at VHost Level.
	//stream_server->SetCrossDomain(cross_domains);

	// Start the HLS Server
	if (stream_server->Start(address, http_server_manager, DEFAULT_SEGMENT_WORKER_THREAD_COUNT,
							 host_info.GetCertificate(), host_info.GetChainCertificate()) == false)
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	return Publisher::Start();
}

std::shared_ptr<pub::Application> HlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (application_info.CheckCodecAvailability({"h264"}, {"aac"}) == false)
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
	}

	return HlsApplication::Create(application_info);
}
