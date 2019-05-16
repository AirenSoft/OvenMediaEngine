//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_application.h"
#include "dash_stream.h"
#include "dash_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<DashApplication> DashApplication::Create(const info::Application *application_info)
{
	auto application = std::make_shared<DashApplication>(application_info);
	application->Start();
	return application;
}

//====================================================================================================
// DashApplication
//====================================================================================================
DashApplication::DashApplication(const info::Application *application_info) : Application(application_info)
{
    auto publisher_info = application_info->GetPublisher<cfg::DashPublisher>();
    _segment_count = publisher_info->GetSegmentCount();
    _segment_duration = publisher_info->GetSegmentDuration();
}

//====================================================================================================
// ~DashApplication
//====================================================================================================
DashApplication::~DashApplication()
{
	Stop();
	logtd("Dash Application(%d) has been terminated finally", GetId());
}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool DashApplication::DeleteStream(std::shared_ptr<StreamInfo> info)
{
	return true;
}

//====================================================================================================
// Start
//====================================================================================================
bool DashApplication::Start()
{
	return Application::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool DashApplication::Stop()
{
	return Application::Stop();
}

//====================================================================================================
// CreateStream
// - Application Override
//====================================================================================================
std::shared_ptr<Stream> DashApplication::CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count)
{
	logtd("Dash CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return DashStream::Create(_segment_count,
                            _segment_duration,
                            GetSharedPtrAs<Application>(),
                            *info.get(),
                            worker_count);
}
