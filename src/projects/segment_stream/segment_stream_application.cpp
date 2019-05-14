//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream_application.h"
#include "segment_stream_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<SegmentStreamApplication> SegmentStreamApplication::Create(cfg::PublisherType publisher_type,
        const info::Application *application_info)
{
	auto application = std::make_shared<SegmentStreamApplication>(publisher_type, application_info);
	application->Start();
	return application;
}

//====================================================================================================
// SegmentStreamApplication
//====================================================================================================
SegmentStreamApplication::SegmentStreamApplication(cfg::PublisherType publisher_type,
        const info::Application *application_info) : Application(application_info)
{
    _publisher_type = publisher_type;
}

//====================================================================================================
// ~SegmentStreamApplication
//====================================================================================================
SegmentStreamApplication::~SegmentStreamApplication()
{
	Stop();
	logtd("SegmentStreamApplication(%d) has been terminated finally", GetId());

}

//====================================================================================================
// DeleteStream
//====================================================================================================
bool SegmentStreamApplication::DeleteStream(std::shared_ptr<StreamInfo> info)
{
	return true;
}

//====================================================================================================
// Start
//====================================================================================================
bool SegmentStreamApplication::Start()
{
	return Application::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool SegmentStreamApplication::Stop()
{
	return Application::Stop();
}

//====================================================================================================
// CreateStream
// - Application Override
// - 세션 기반이 아니라 Stream 생성 필요 없음
// - 스트림 별 Stream Packetyzer 생성
// -
//====================================================================================================
std::shared_ptr<Stream> SegmentStreamApplication::CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count)
{
	// Stream Class 생성할때는 복사를 사용한다.
	logtd("CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return SegmentStream::Create(GetSharedPtrAs<Application>(), _publisher_type, *info.get(), worker_count);
}
