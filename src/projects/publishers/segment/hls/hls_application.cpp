//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_application.h"

#include "../segment_stream/segment_stream.h"
#include "hls_packetizer.h"
#include "hls_private.h"
#include "hls_publisher.h"

std::shared_ptr<HlsApplication> HlsApplication::Create(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info)
{
	auto application = std::make_shared<HlsApplication>(publisher, application_info);

	if (application->Start() == false)
	{
		return nullptr;
	}

	return application;
}

HlsApplication::HlsApplication(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info)
	: Application(publisher, application_info)
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
	logtd("HLS Stream is created: %s/%u", info->GetName().CStr(), info->GetId());

	auto server_config = cfg::ConfigManager::GetInstance()->GetServer();

	return SegmentStream::Create(
		GetSharedPtrAs<pub::Application>(), *info.get(),
		[=](ov::String app_name, ov::String stream_name,
			std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) -> std::shared_ptr<Packetizer> {
			return std::make_shared<HlsPacketizer>(
				server_config->GetName(), app_name, stream_name,
				_segment_count, _segment_duration,
				video_track, audio_track,
				nullptr);
		}, _segment_duration);
}

bool HlsApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("HLS Stream is deleted: %s/%u", info->GetName().CStr(), info->GetId());

	// Nothing to do
	return true;
}
