//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "packetizer.h"

#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/h264/h264_converter.h>

#include <algorithm>
#include <sstream>

#include "../segment_stream_private.h"

Packetizer::Packetizer(const ov::String &service_name, const ov::String &app_name, const ov::String &stream_name,
					   uint32_t segment_count, uint32_t segment_save_count, uint32_t segment_duration,
					   const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
					   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)

	: _service_name(service_name), _app_name(app_name), _stream_name(stream_name),

	  _segment_count(segment_count),
	  _segment_save_count(segment_save_count),
	  _segment_duration(segment_duration),

	  _video_track(video_track),
	  _audio_track(audio_track),

	  _chunked_transfer(chunked_transfer),

	  _video_segment_queue(this, app_name, stream_name, _video_track, segment_count, segment_save_count),
	  _audio_segment_queue(this, app_name, stream_name, _audio_track, segment_count, segment_save_count)
{
}

uint64_t Packetizer::ConvertTimeScale(uint64_t time, const cmn::Timebase &from_timebase, const cmn::Timebase &to_timebase)
{
	if (from_timebase.GetExpr() == 0.0)
	{
		return 0;
	}

	double ratio = from_timebase.GetExpr() / to_timebase.GetExpr();

	return (uint64_t)((double)time * ratio);
}

void Packetizer::SetPlayList(const ov::String &play_list)
{
	std::unique_lock<std::mutex> lock(_play_list_mutex);

	_play_list = play_list;
}

bool Packetizer::IsReadyForStreaming() const noexcept
{
	return _streaming_start;
}

void Packetizer::SetReadyForStreaming() noexcept
{
	_streaming_start = true;
}

ov::String Packetizer::GetCodecString(const std::shared_ptr<const MediaTrack> &track)
{
	return track->GetCodecsParameter();
}

bool Packetizer::GetPlayList(ov::String &play_list)
{
	if (IsReadyForStreaming() == false)
	{
		logtd("Manifest was requested before the stream began");
		return false;
	}

	std::unique_lock<std::mutex> lock(_play_list_mutex);
	play_list = _play_list;

	return true;
}
