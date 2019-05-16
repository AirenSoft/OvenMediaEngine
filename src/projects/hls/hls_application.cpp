//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_application.h"
#include "hls_stream.h"
#include "hls_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<HlsApplication> HlsApplication::Create(const info::Application *application_info)
{
	auto application = std::make_shared<HlsApplication>(application_info);
	application->Start();
	return application;
}

//====================================================================================================
// DashApplication
//====================================================================================================
HlsApplication::HlsApplication(const info::Application *application_info) : Application(application_info)
{
    auto publisher_info = application_info->GetPublisher<cfg::HlsPublisher>();
    _segment_count = publisher_info->GetSegmentCount();
    _segment_duration = publisher_info->GetSegmentDuration();
}

//====================================================================================================
// ~DashApplication
//====================================================================================================
HlsApplication::~HlsApplication()
{
	Stop();
	logtd("Hls Application(%d) has been terminated finally", GetId());
}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool HlsApplication::DeleteStream(std::shared_ptr<StreamInfo> info)
{
	return true;
}

//====================================================================================================
// Start
//====================================================================================================
bool HlsApplication::Start()
{
	return Application::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool HlsApplication::Stop()
{
	return Application::Stop();
}

//====================================================================================================
// CreateStream
// - Application Override
//====================================================================================================
std::shared_ptr<Stream> HlsApplication::CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count)
{
	logtd("Hls CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return HlsStream::Create(_segment_count,
                             _segment_duration,
                             GetSharedPtrAs<Application>(),
                             *info.get(),
                             worker_count);
}
