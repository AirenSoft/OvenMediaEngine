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
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<CmafPublisher>(http_server_manager, server_config, router);
}

CmafPublisher::CmafPublisher(PrivateToken token,
							const cfg::Server &server_config,
							const std::shared_ptr<MediaRouteInterface> &router)
	: DashPublisher(token, server_config, router)
{
}

bool CmafPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	return SegmentPublisher::Start(http_server_manager,
								   GetServerConfig().GetBind().GetPublishers().GetDash(),
								   std::make_shared<CmafStreamServer>());
}

std::shared_ptr<pub::Application> CmafPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	/* Deprecated
	if (!application_info.CheckCodecAvailability({"h264"}, {"aac"}))
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
		// return nullptr;
	}
	*/

	return CmafApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info, std::static_pointer_cast<CmafStreamServer>(_stream_server));
}

bool CmafPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}