//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_application.h"
#include "cmaf_stream.h"
#include "cmaf_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<CmafApplication> CmafApplication::Create(const info::Application &application_info,
		const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
{
	auto application = std::make_shared<CmafApplication>(application_info, chunked_transfer);
	if(!application->Start())
	{
		return nullptr;
	}
	return application;
}

//====================================================================================================
// CmafApplication
//====================================================================================================
CmafApplication::CmafApplication(const info::Application &application_info,
								const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
									: Application(application_info)
{
    // 3, 2 are default values
    _segment_count = 3;
    _segment_duration = 2;
	_chunked_transfer = chunked_transfer;
}

//====================================================================================================
// ~CmafApplication
//====================================================================================================
CmafApplication::~CmafApplication()
{
	Stop();
	logtd("Cmaf Application(%d) has been terminated finally", GetId());
}

//====================================================================================================
// Start
//====================================================================================================
bool CmafApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::CmafPublisher>();
	// This application doesn't enable LLDASH
	if(publisher_info == nullptr)
	{
		return false;
	}

	_segment_count = publisher_info->GetSegmentCount();
	_segment_duration = publisher_info->GetSegmentDuration();

	return Application::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool CmafApplication::Stop()
{
	return Application::Stop();
}

//====================================================================================================
// CreateStream
// - Application Override
//====================================================================================================
std::shared_ptr<Stream> CmafApplication::CreateStream(const std::shared_ptr<StreamInfo> &info, uint32_t thread_count)
{
	logtd("Cmaf CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return CmafStream::Create(_segment_count,
                            _segment_duration,
                            GetSharedPtrAs<Application>(),
                            *info.get(),
                            thread_count,
							_chunked_transfer);
}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool CmafApplication::DeleteStream(const std::shared_ptr<StreamInfo> &info)
{
	return true;
}
