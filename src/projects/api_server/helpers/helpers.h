//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <orchestrator/orchestrator.h>

namespace api
{
	std::shared_ptr<ocst::VirtualHost> GetVirtualHost(const std::string_view &vhost_name);
	std::shared_ptr<ocst::Application> GetApplication(const std::shared_ptr<ocst::VirtualHost> &vhost, const std::string_view &app_name);
}  // namespace api