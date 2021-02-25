//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"
#include "base/provider/application.h"
#include "base/provider/stream.h"


namespace pvd
{
	class WebRTCApplication : public pvd::Application
	{
	
	public:
		static std::shared_ptr<WebRTCApplication> Create(const std::shared_ptr<pvd::Provider> &provider, const info::Application &application_info);

		explicit WebRTCApplication(const std::shared_ptr<pvd::Provider> &provider, const info::Application &info);
		~WebRTCApplication() override = default;
	};
}