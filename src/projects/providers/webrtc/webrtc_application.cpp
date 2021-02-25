//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_application.h"

namespace pvd
{
	std::shared_ptr<WebRTCApplication> WebRTCApplication::Create(const std::shared_ptr<pvd::Provider> &provider, const info::Application &application_info)
	{
		auto application = std::make_shared<WebRTCApplication>(provider, application_info);
		application->Start();
		return application;
	}

	WebRTCApplication::WebRTCApplication(const std::shared_ptr<pvd::Provider> &provider, const info::Application &application_info)
		: Application(provider, application_info)
	{
	}
}