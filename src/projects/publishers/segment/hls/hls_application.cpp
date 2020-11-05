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
std::shared_ptr<HlsApplication> HlsApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<HlsApplication>(publisher, application_info);
	if(!application->Start())
	{
		return nullptr;
	}
	return application;
}

//====================================================================================================
// HlsApplication
//====================================================================================================
HlsApplication::HlsApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{
}

//====================================================================================================
// ~HlsApplication
//====================================================================================================
HlsApplication::~HlsApplication()
{
	Stop();
	logtd("Hls Application(%d) has been terminated finally", GetId());
}

//====================================================================================================
// Start
//====================================================================================================
bool HlsApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::HlsPublisher>();

	_segment_count = publisher_info->GetSegmentCount();
	_segment_duration = publisher_info->GetSegmentDuration();

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
std::shared_ptr<pub::Stream> HlsApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	logtd("Hls CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return HlsStream::Create(_segment_count,
                             _segment_duration,
                             GetSharedPtrAs<pub::Application>(),
                             *info.get(),
							 thread_count);
}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool HlsApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	return true;
}
