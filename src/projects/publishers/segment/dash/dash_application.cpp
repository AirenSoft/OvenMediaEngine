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
std::shared_ptr<DashApplication> DashApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<DashApplication>(publisher, application_info);
	if(!application->Start())
	{
		return nullptr;
	}
	return application;
}

//====================================================================================================
// DashApplication
//====================================================================================================
DashApplication::DashApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{
    auto publisher_info = application_info.GetPublisher<cfg::vhost::app::pub::DashPublisher>();
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
// Start
//====================================================================================================
bool DashApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::DashPublisher>();
	
	_segment_count = publisher_info->GetSegmentCount();
	_segment_duration = publisher_info->GetSegmentDuration();

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
std::shared_ptr<pub::Stream> DashApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	logtd("Dash CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return DashStream::Create(_segment_count,
                            _segment_duration,
                            GetSharedPtrAs<pub::Application>(),
                            *info.get(),
                            thread_count);
}


//====================================================================================================
// DeleteStream
//====================================================================================================
bool DashApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	return true;
}
