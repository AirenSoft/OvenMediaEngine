//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_application.h"
#include "rtmp_stream.h"

#include "base/provider/push_provider/application.h"
#include "base/info/stream.h"

#define OV_LOG_TAG "RtmpApplication"

namespace pvd
{
	std::shared_ptr<RtmpApplication> RtmpApplication::Create(const std::shared_ptr<PushProvider> &provider, const info::Application &application_info)
	{
		auto application = std::make_shared<RtmpApplication>(provider, application_info);
		application->Start();
		return application;
	}

	RtmpApplication::RtmpApplication(const std::shared_ptr<PushProvider> &provider, const info::Application &application_info)
		: PushApplication(provider, application_info)
	{
	}
}