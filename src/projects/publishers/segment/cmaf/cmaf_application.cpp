//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_application.h"

#include "../segment_stream/segment_stream.h"
#include "cmaf_packetizer.h"
#include "cmaf_private.h"

std::shared_ptr<CmafApplication> CmafApplication::Create(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info,
	const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
{
	auto application = std::make_shared<CmafApplication>(publisher, application_info, chunked_transfer);

	if (application->Start() == false)
	{
		return nullptr;
	}

	return application;
}

CmafApplication::CmafApplication(
	const std::shared_ptr<pub::Publisher> &publisher,
	const info::Application &application_info,
	const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Application(publisher, application_info)
{
	_chunked_transfer = chunked_transfer;
}

CmafApplication::~CmafApplication()
{
	Stop();
}

bool CmafApplication::Start()
{
	auto publisher_info = GetPublisher<cfg::vhost::app::pub::LlDashPublisher>();

	_segment_count = 1;
	_segment_duration = publisher_info->GetSegmentDuration();

	auto &utc_timing = publisher_info->GetUtcTiming();

	if (utc_timing.IsParsed())
	{
		_utc_timing_scheme = utc_timing.GetScheme();
		_utc_timing_value = utc_timing.GetValue();
	}

	return Application::Start();
}

std::shared_ptr<pub::Stream> CmafApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count)
{
	logtd("LL-DASH Stream is created: %s/%u", info->GetName().CStr(), info->GetId());
	auto server_config = cfg::ConfigManager::GetInstance()->GetServer();

	return SegmentStream::Create(
		GetSharedPtrAs<pub::Application>(), *info.get(),
		[=](ov::String app_name, ov::String stream_name,
			std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) -> std::shared_ptr<Packetizer> {
			return std::make_shared<CmafPacketizer>(
				server_config->GetName(), app_name, stream_name,
				_segment_count, _segment_duration,
				_utc_timing_scheme, _utc_timing_value,
				video_track, audio_track,
				_chunked_transfer);
		}, _segment_duration);
}

bool CmafApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("LL-DASH Stream is deleted: %s/%u", info->GetName().CStr(), info->GetId());

	// Nothing to do
	return true;
}
