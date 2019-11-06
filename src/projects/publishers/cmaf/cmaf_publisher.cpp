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
													 const info::Application *application_info,
													 std::shared_ptr<MediaRouteInterface> router)
{
	return SegmentPublisher::Create<CmafPublisher>(http_server_manager, application_info, std::move(router));
}

CmafPublisher::CmafPublisher(PrivateToken token, const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: DashPublisher(token, application_info, std::move(router))
{
}

bool CmafPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto host = _application_info->GetParentAs<cfg::Host>("Host");
	auto port = host->GetPorts().GetCmafPort();
	auto publisher_info = _application_info->GetPublisher<cfg::CmafPublisher>();

	return StartInternal(http_server_manager, port.GetPort(), std::make_shared<CmafStreamServer>(),
						 publisher_info->GetCrossDomains(),
						 publisher_info->GetSegmentCount(), publisher_info->GetSegmentDuration(),
						 publisher_info->GetThreadCount());
}

std::shared_ptr<Application> CmafPublisher::OnCreateApplication(const info::Application &application_info)
{
	return CmafApplication::Create(application_info, std::static_pointer_cast<CmafStreamServer>(_stream_server));
}
