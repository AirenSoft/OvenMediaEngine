//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
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
	: SegmentPublisher(application_info, std::move(router))
{
}

bool CmafPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	if (CheckCodecAvailability({"h264"}, {"aac"}) == false)
	{
		return false;
	}

	auto host = _application_info->GetParentAs<cfg::Host>("Host");
	auto port = host->GetPorts().GetCmafPort();
	ov::SocketAddress address(host->GetIp(), port.GetPort());

	auto stream_server = std::make_shared<CmafStreamServer>();
	auto publisher_info = _application_info->GetPublisher<cfg::CmafPublisher>();

	// Register as observer
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	stream_server->SetCrossDomain(publisher_info->GetCrossDomains());

	// Start the CMAF Server
	if (stream_server->Start(
			address,
			http_server_manager,
			_application_info->GetName(),
			publisher_info->GetThreadCount(),
			_application_info->GetCertificate(),
			_application_info->GetChainCertificate()) == false)
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	logtd(
		"%s Publisher is created successfully on %s"
		"(SegmentCount: %d, SegmentDuration: %d, Threads: %d)",
		GetPublisherName(),
		address.ToString().CStr(),
		publisher_info->GetSegmentCount(),
		publisher_info->GetSegmentDuration(),
		publisher_info->GetThreadCount());

	return Publisher::Start();
}

std::shared_ptr<Application> CmafPublisher::OnCreateApplication(const info::Application *application_info)
{
	return CmafApplication::Create(application_info, std::static_pointer_cast<CmafStreamServer>(_stream_server));
}
