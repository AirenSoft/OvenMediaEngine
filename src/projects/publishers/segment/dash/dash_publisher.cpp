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

std::shared_ptr<DashPublisher> DashPublisher::Create(const cfg::Server &server_config,
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<DashPublisher>(server_config, router);
}

DashPublisher::DashPublisher(PrivateToken token,
							 const cfg::Server &server_config,
							 const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, router)
{
}

bool DashPublisher::Start()
{
	auto &dash_config = GetServerConfig().GetBind().GetPublishers().GetDash();

	if (dash_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_parsed;
	auto worker_count = dash_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	return SegmentPublisher::Start(dash_config.GetPort(), dash_config.GetTlsPort(),
								   std::make_shared<DashStreamServer>(), worker_count);
}

std::shared_ptr<pub::Application> DashPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if(IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return DashApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool DashPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}