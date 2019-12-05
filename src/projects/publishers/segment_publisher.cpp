//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_publisher.h"
#include "publisher_private.h"

#include <segment_stream/segment_stream.h>

SegmentPublisher::SegmentPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: Publisher(application_info, std::move(router))
{
}

SegmentPublisher::~SegmentPublisher()
{
	logtd("Publisher has been destroyed");
}

bool SegmentPublisher::CheckCodecAvailability(const std::vector<ov::String> &video_codecs, const std::vector<ov::String> &audio_codecs)
{
	for (const auto &stream : _application_info->GetStreamList())
	{
		const auto &profiles = stream.GetProfiles();

		for (const auto &profile : profiles)
		{
			cfg::StreamProfileUse use = profile.GetUse();
			ov::String profile_name = profile.GetName();

			if (_application_info->HasEncodeWithCodec(profile_name, use, video_codecs, audio_codecs))
			{
				_is_codec_available = true;
				return _is_codec_available;
			}
		}
	}

	logtw("There is no suitable encoding setting for %s (Encoding setting must contains %s or %s)",
		  GetPublisherName(), ov::String::Join(video_codecs, ", ").CStr(), ov::String::Join(audio_codecs, ", ").CStr());

	_is_codec_available = false;
	
	return _is_codec_available;
}

bool SegmentPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
	return (_stream_server != nullptr) ? _stream_server->GetMonitoringCollectionData(collections) : false;
}

bool SegmentPublisher::OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name,
										 const ov::String &file_name,
										 ov::String &play_list)
{
	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if ((stream == nullptr) || (stream->GetPlayList(play_list) == false))
	{
		logtw("Could not get a playlist for %s [%p, %s/%s, %s]", GetPublisherName(), stream.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return true;
}

bool SegmentPublisher::OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name,
										std::shared_ptr<SegmentData> &segment)
{
	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if (stream != nullptr)
	{
		segment = stream->GetSegmentData(file_name);

		if (segment == nullptr)
		{
			logtw("Could not find a segment for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
		else if (segment->data == nullptr)
		{
			logtw("Could not obtain segment data from %s for [%p, %s/%s, %s]", GetPublisherName(), segment.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
	}
	else
	{
		logtw("Could not find a stream for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return true;
}
