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

#include "base/provider/application.h"
#include "base/application/application_info.h"
#include "base/application/stream_info.h"

#define OV_LOG_TAG "RtmpApplication"

std::shared_ptr<RtmpApplication> RtmpApplication::Create(const ApplicationInfo &info)
{
	auto application = std::make_shared<RtmpApplication>(info);
	return application;
}

RtmpApplication::RtmpApplication(const ApplicationInfo &info)
	:Application(info)
{
}

RtmpApplication::~RtmpApplication()
{
	
}

std::shared_ptr<Stream> RtmpApplication::OnCreateStream()
{
	logtd("OnCreateStream");

	auto stream = RtmpStream::Create();

	return stream;
}
