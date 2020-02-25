//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_publisher.h"
#include "cmaf_application.h"
#include "cmaf_private.h"
#include "cmaf_stream_server.h"

#include <config/config_manager.h>

std::shared_ptr<CmafPublisher> CmafPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
													 const cfg::Server &server_config,
													 const info::Host &host_info,
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<CmafPublisher>(http_server_manager, server_config, host_info, router);
}

CmafPublisher::CmafPublisher(PrivateToken token,
							const cfg::Server &server_config,
							const info::Host &host_info,
							const std::shared_ptr<MediaRouteInterface> &router)
	: DashPublisher(token, server_config, host_info, router)
{
}

bool CmafPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto server_config = GetServerConfig();
	auto host_info = GetHostInfo();
	auto ip = server_config.GetIp();
	auto port = server_config.GetBind().GetPublishers().GetDashPort();

	ov::SocketAddress address(ip, port);

	// Register as observer
	auto stream_server = std::make_shared<CmafStreamServer>();
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	// TODO(Dimiden): The Cross Domain configure must be at VHost Level.
	//stream_server->SetCrossDomain(cross_domains);

	// Start the HLS Server
	if (!stream_server->Start(address, http_server_manager, DEFAULT_SEGMENT_WORKER_THREAD_COUNT,
							  host_info.GetCertificate(), host_info.GetChainCertificate()))
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	return Publisher::Start();
}

std::shared_ptr<pub::Application> CmafPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (!application_info.CheckCodecAvailability({"h264"}, {"aac"}))
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
		// return nullptr;
	}

	return CmafApplication::Create(application_info, std::static_pointer_cast<CmafStreamServer>(_stream_server));
}
