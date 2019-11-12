//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publishers/segment/segment_publisher.h"

class DashPublisher : public SegmentPublisher
{
public:
	static std::shared_ptr<DashPublisher> Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
												 const info::Application *application_info,
												 std::shared_ptr<MediaRouteInterface> router);

	DashPublisher(PrivateToken token, const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

protected:
	bool StartInternal(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager, int port,
					   const std::shared_ptr<SegmentStreamServer> &stream_server, const std::vector<cfg::Url> &cross_domains,
					   int segment_count, int segment_duration, int thread_count);

	//--------------------------------------------------------------------
	// Implementation of SegmentPublisher
	//--------------------------------------------------------------------
	bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	std::shared_ptr<Application> OnCreatePublisherApplication(const info::Application &application_info) override;

	cfg::PublisherType GetPublisherType() const override
	{
		return cfg::PublisherType::Dash;
	}

	const char *GetPublisherName() const override
	{
		return "DASH";
	}
};
