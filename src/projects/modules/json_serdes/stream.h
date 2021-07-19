//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <monitoring/monitoring.h>
namespace serdes
{
	Json::Value JsonFromTracks(const std::map<int32_t, std::shared_ptr<MediaTrack>> &tracks);
	Json::Value JsonFromStream(const std::shared_ptr<const mon::StreamMetrics> &stream);
	
	Json::Value JsonFromStream(const std::shared_ptr<const mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);
}  // namespace serdes