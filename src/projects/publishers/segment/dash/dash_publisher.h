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
	static std::shared_ptr<DashPublisher> Create(const cfg::Server &server_config,
												 const std::shared_ptr<MediaRouteInterface> &router);

	DashPublisher(PrivateToken token, const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

protected:
	//--------------------------------------------------------------------
	// Implementation of SegmentPublisher
	//--------------------------------------------------------------------
	bool Start() override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

	PublisherType GetPublisherType() const override
	{
		return PublisherType::Dash;
	}

	const char *GetPublisherName() const override
	{
		return "DASH Publisher";
	}
};
