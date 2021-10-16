//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_publisher.h"

#include <config/config_manager.h>

#include "cmaf_application.h"
#include "cmaf_private.h"
#include "cmaf_stream_server.h"

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
	// LL-DASH uses DASH port
	auto dash_config = GetServerConfig().GetBind().GetPublishers().GetDash();

	if (dash_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_parsed;
	auto worker_count = dash_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	return SegmentPublisher::Start(dash_config.GetPort(), dash_config.GetTlsPort(),
								   std::make_shared<CmafStreamServer>(), worker_count);
}

std::shared_ptr<pub::Application> CmafPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	auto name = application_info.GetName();
	auto &lldash_publisher_config = application_info.GetConfig().GetPublishers().GetLlDashPublisher();

	if (lldash_publisher_config.IsParsed() == false)
	{
		logte("Could not find %s configuration for application %s", GetPublisherName(), name.CStr());
		return nullptr;
	}

	auto application = CmafApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info, std::static_pointer_cast<CmafStreamServer>(_stream_server));

	if (application != nullptr)
	{
		auto stream_server = _stream_server;

		if (stream_server != nullptr)
		{
			bool is_parsed;
			auto cross_domains = lldash_publisher_config.GetCrossDomainList(&is_parsed);

			if (is_parsed == false)
			{
				OV_ASSERT2(cross_domains.empty());
				cross_domains.push_back("*");
			}

			stream_server->SetCrossDomains(name, cross_domains);
		}
	}

	return application;
}

bool CmafPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}