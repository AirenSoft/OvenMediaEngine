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
std::shared_ptr<CmafApplication> CmafApplication::Create(const info::Application *application_info,
		const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
{
	auto application = std::make_shared<CmafApplication>(application_info, chunked_transfer);
	application->Start();



	return application;
}

//====================================================================================================
// CmafApplication
//====================================================================================================
CmafApplication::CmafApplication(const info::Application *application_info,
								const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
									: Application(application_info)
{
    auto publisher_info = application_info->GetPublisher<cfg::CmafPublisher>();
    _segment_count = publisher_info->GetSegmentCount();
    _segment_duration = publisher_info->GetSegmentDuration();
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
// DeleteStream
//====================================================================================================
bool CmafApplication::DeleteStream(std::shared_ptr<StreamInfo> info)
{
	return true;
}

//====================================================================================================
// Start
//====================================================================================================
bool CmafApplication::Start()
{
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
std::shared_ptr<Stream> CmafApplication::CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count)
{
	logtd("Cmaf CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return CmafStream::Create(_segment_count,
                            _segment_duration,
                            GetSharedPtrAs<Application>(),
                            *info.get(),
                            worker_count,
							  _chunked_transfer);
}
