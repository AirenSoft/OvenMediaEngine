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
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<DashPublisher>(http_server_manager, server_config, router);
}

DashPublisher::DashPublisher(PrivateToken token,
							 const cfg::Server &server_config,
							 const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, router)
{
}

bool DashPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	return SegmentPublisher::Start(http_server_manager,
								   GetServerConfig().GetBind().GetPublishers().GetDash(),
								   std::make_shared<DashStreamServer>());
}

std::shared_ptr<pub::Application> DashPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	/* Deprecated
	if (!application_info.CheckCodecAvailability({"h264"}, {"aac"}))
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
		// return nullptr;
	}
	*/

	return DashApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool DashPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}