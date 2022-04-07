//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_publisher.h"

#include <config/config_manager.h>

#include "dash_application.h"
#include "dash_private.h"
#include "dash_stream_server.h"

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
								   std::make_shared<DashStreamServer>(), false, worker_count);
}

std::shared_ptr<pub::Application> DashPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	auto name = application_info.GetName();
	auto &dash_publisher_config = application_info.GetConfig().GetPublishers().GetDashPublisher();

	if (dash_publisher_config.IsParsed() == false)
	{
		logte("Could not find %s configuration for application %s", GetPublisherName(), name.CStr());
		return nullptr;
	}

	auto application = DashApplication::Create(pub::Publisher::GetSharedPtrAs<DashPublisher>(), application_info);

	if (application != nullptr)
	{
		auto stream_server = _stream_server;

		if (stream_server != nullptr)
		{
			bool is_parsed;
			const auto &cross_domains = dash_publisher_config.GetCrossDomainList(&is_parsed);

			if (is_parsed)
			{
				stream_server->SetCrossDomains(name, cross_domains);
			}
		}
	}

	return application;
}

bool DashPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}