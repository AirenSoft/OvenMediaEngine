//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publishers/segment/segment_publisher.h"

class HlsPublisher : public SegmentPublisher
{
public:
	static std::shared_ptr<HlsPublisher> Create(const std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
												const info::Application &application_info,
												const std::shared_ptr<MediaRouteInterface> &router);

	HlsPublisher(PrivateToken token, const info::Application &application_info, const std::shared_ptr<MediaRouteInterface> &router);

protected:
	//--------------------------------------------------------------------
	// Implementation of SegmentPublisher
	//--------------------------------------------------------------------
	bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	std::shared_ptr<Application> OnCreatePublisherApplication(const info::Application &application_info) override;

	PublisherType GetPublisherType() const override
	{
		return PublisherType::Hls;
	}

	const char *GetPublisherName() const override
	{
		return "HLS";
	}
};
