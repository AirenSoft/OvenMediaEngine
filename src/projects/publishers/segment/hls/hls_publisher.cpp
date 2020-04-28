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
												   const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<HlsPublisher>(http_server_manager, server_config, router);
}

HlsPublisher::HlsPublisher(PrivateToken token,
						   const cfg::Server &server_config,
						   const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, router)
{
}

bool HlsPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	return SegmentPublisher::Start(http_server_manager,
								   GetServerConfig().GetBind().GetPublishers().GetHls(),
								   std::make_shared<HlsStreamServer>());
}

std::shared_ptr<pub::Application> HlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	/* Deprecated
	if (application_info.CheckCodecAvailability({"h264"}, {"aac"}) == false)
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
	}
	*/
	return HlsApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool HlsPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}