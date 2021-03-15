//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_publisher.h"

#include <config/config_manager.h>
#include <modules/signature/signed_token.h>
#include <orchestrator/orchestrator.h>

#include "hls_application.h"
#include "hls_private.h"
#include "hls_stream_server.h"

std::shared_ptr<HlsPublisher> HlsPublisher::Create(const cfg::Server &server_config,
												   const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<HlsPublisher>(server_config, router);
}

HlsPublisher::HlsPublisher(PrivateToken token,
						   const cfg::Server &server_config,
						   const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, router)
{
}

bool HlsPublisher::Start()
{
	auto &hls_config = GetServerConfig().GetBind().GetPublishers().GetHls();

	if (hls_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_parsed;
	auto worker_count = hls_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	return SegmentPublisher::Start(hls_config.GetPort(), hls_config.GetTlsPort(),
								   std::make_shared<HlsStreamServer>(), worker_count);
}

std::shared_ptr<pub::Application> HlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return HlsApplication::Create(pub::Publisher::GetSharedPtrAs<HlsPublisher>(), application_info);
}

bool HlsPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}