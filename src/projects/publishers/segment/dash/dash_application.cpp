//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_application.h"

#include <base/publisher/publisher.h>

#include "../segment_stream/segment_stream.h"
#include "dash_packetizer.h"
#include "dash_private.h"

std::shared_ptr<DashApplication> DashApplication::Create(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info)
{
	auto application = std::make_shared<DashApplication>(publisher, application_info);

	if (application->Start() == false)
	{
		return nullptr;
	}

	return application;
}

DashApplication::DashApplication(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info)
	: Application(publisher, application_info)
{
}

DashApplication::~DashApplication()
{
	Stop();
}

bool DashApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::DashPublisher>();

	_segment_count = publisher_info->GetSegmentCount();
	_segment_duration = publisher_info->GetSegmentDuration();

	auto &utc_timing = publisher_info->GetUtcTiming();

	if (utc_timing.IsParsed())
	{
		_utc_timing_scheme = utc_timing.GetScheme();
		_utc_timing_value = utc_timing.GetValue();
	}

	return Application::Start();
}

std::shared_ptr<pub::Stream> DashApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	logtd("DASH Stream is created: %s/%u", info->GetName().CStr(), info->GetId());
	auto server_config = cfg::ConfigManager::GetInstance()->GetServer();

	return SegmentStream::Create(
		GetSharedPtrAs<pub::Application>(), *info.get(),
		[=](ov::String app_name, ov::String stream_name,
			std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) -> std::shared_ptr<Packetizer> {
			return std::make_shared<DashPacketizer>(
				server_config->GetName(), app_name, stream_name,
				_segment_count, _segment_duration,
				_utc_timing_scheme, _utc_timing_value,
				video_track, audio_track,
				nullptr);
		}, _segment_duration);
}

bool DashApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("DASH Stream is deleted: %s/%u", info->GetName().CStr(), info->GetId());

	// Nothing to do
	return true;
}
