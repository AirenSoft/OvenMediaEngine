//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../segment_publisher.h"

class HlsPublisher : public SegmentPublisher
{
public:
	static std::shared_ptr<HlsPublisher> Create(const cfg::Server &server_config,
												const std::shared_ptr<MediaRouteInterface> &router);

	HlsPublisher(PrivateToken token, const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

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
		return PublisherType::Hls;
	}

	const char *GetPublisherName() const override
	{
		return "HLS Publisher";
	}
};
