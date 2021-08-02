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
#include <monitoring/monitoring.h>
namespace serdes
{
	Json::Value JsonFromVHost(const std::shared_ptr<const mon::HostMetrics> &vhost);
}  // namespace serdes