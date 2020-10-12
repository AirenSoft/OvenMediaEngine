//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/publisher.h>

struct SegmentStreamRequestInfo
{
	info::VHostAppName vhost_app_name;
	ov::String host_name;
	ov::String app_name;
	ov::String stream_name;
	ov::String file_name;

	SegmentStreamRequestInfo(info::VHostAppName vhost_app_name,
							 ov::String host_name,
							 ov::String app_name,
							 ov::String stream_name,
							 ov::String file_name)
		: vhost_app_name(vhost_app_name),
		  host_name(host_name),
		  app_name(app_name),
		  stream_name(stream_name),
		  file_name(file_name)
	{
	}
};
