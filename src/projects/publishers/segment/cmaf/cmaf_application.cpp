//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_application.h"

#include "../segment_stream/segment_stream.h"
#include "cmaf_private.h"
#include "cmaf_stream_packetizer.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<CmafApplication> CmafApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info,
														 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
{
	auto application = std::make_shared<CmafApplication>(publisher, application_info, chunked_transfer);
	if (!application->Start())
	{
		return nullptr;
	}
	return application;
}

//====================================================================================================
// CmafApplication
//====================================================================================================
CmafApplication::CmafApplication(const std::shared_ptr<pub::Publisher> &publisher,
								 const info::Application &application_info,
								 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Application(publisher, application_info)
{
	auto publisher_info = application_info.GetPublisher<cfg::vhost::app::pub::LlDashPublisher>();
	_segment_count = 1;
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
// Start
//====================================================================================================
bool CmafApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::LlDashPublisher>();

	_segment_count = 1;
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
std::shared_ptr<pub::Stream> CmafApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	logtd("Cmaf CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return SegmentStream::Create<CmafStreamPacketizer>(
		GetSharedPtrAs<pub::Application>(), *info.get(),
		_segment_count, _segment_duration,
		thread_count,
		_chunked_transfer);
}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool CmafApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	return true;
}
