//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_application.h"

#include "../segment_stream/segment_stream.h"
#include "hls_private.h"
#include "hls_publisher.h"
#include "hls_stream_packetizer.h"

std::shared_ptr<HlsApplication> HlsApplication::Create(const std::shared_ptr<HlsPublisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<HlsApplication>(publisher, application_info);

	if (application->Start() == false)
	{
		return nullptr;
	}

	return application;
}

HlsApplication::HlsApplication(const std::shared_ptr<HlsPublisher> &publisher, const info::Application &application_info)
	: Application(publisher->pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info)
{
}

HlsApplication::~HlsApplication()
{
	Stop();
}

bool HlsApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::HlsPublisher>();

	_segment_count = publisher_info->GetSegmentCount();
	_segment_duration = publisher_info->GetSegmentDuration();

	return Application::Start();
}

std::shared_ptr<pub::Stream> HlsApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	return SegmentStream::Create<HlsStreamPacketizer>(
		GetSharedPtrAs<pub::Application>(), *info.get(),
		_segment_count, _segment_duration,
		thread_count,
		nullptr);
}

bool HlsApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	// Nothing to do
	return true;
}
