//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publishers/segment/dash/dash_publisher.h"

// TODO(Dimiden) : Change class name such as LLDashPublisher or CmafDashPublisher
class CmafPublisher : public DashPublisher
{
public:
	static std::shared_ptr<CmafPublisher> Create(const cfg::Server &server_config,
												 const std::shared_ptr<MediaRouteInterface> &router);

	CmafPublisher(PrivateToken token, const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

private:
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
		return PublisherType::LlDash;
	}

	const char *GetPublisherName() const override
	{
		return "LLDASH Publisher";
	}
};
