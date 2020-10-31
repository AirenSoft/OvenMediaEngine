//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <orchestrator/orchestrator.h>

namespace api
{
	namespace conv
	{
		Json::Value ConvertFromVHost(const std::shared_ptr<const ocst::VirtualHost> &vhost);
	}  // namespace conv
};	   // namespace api