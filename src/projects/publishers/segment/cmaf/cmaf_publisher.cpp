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

std::shared_ptr<CmafPublisher> CmafPublisher::Create(const cfg::Server &server_config,
													 const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<CmafPublisher>(server_config, router);
}

CmafPublisher::CmafPublisher(PrivateToken token,
							const cfg::Server &server_config,
							const std::shared_ptr<MediaRouteInterface> &router)
	: DashPublisher(token, server_config, router)
{
}

bool CmafPublisher::Start()
{
	auto dash_config = GetServerConfig().GetBind().GetPublishers().GetDash();

	if (dash_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	return SegmentPublisher::Start(dash_config.GetPort(), dash_config.GetTlsPort(),
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