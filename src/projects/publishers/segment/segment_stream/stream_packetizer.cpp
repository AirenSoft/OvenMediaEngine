//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stream_packetizer.h"
#include "segment_stream_private.h"

bool StreamPacketizer::AppendVideoData(std::shared_ptr<EncodedFrame> encoded_frame,
									   uint32_t timescale,
									   uint64_t time_offset)
{
	if (_stream_type == PacketizerStreamType::AudioOnly)
	{
		OV_ASSERT2("Since it is audio-only, video data cannot be appended");
		return true;
	}

	if (_video_track == nullptr)
	{
		OV_ASSERT2(_video_track != nullptr);
		return false;
	}

	OV_ASSERT2(
		(encoded_frame->_frame_type == FrameType::VideoFrameKey) ||
		(encoded_frame->_frame_type == FrameType::VideoFrameDelta));

	// Converting the timestamp using _video_timescale if needed
	// if (encoded_frame->_timebase != _video_track->GetTimeBase())
	// {
	//		encoded_frame->_time_stamp = Packetizer::ConvertTimeScale(encoded_frame->_time_stamp, timescale, _video_timescale);
	//encoded_frame->_duration = Packetizer::ConvertTimeScale(encoded_frame->_duration, timescale, _video_timescale);
	//time_offset = Packetizer::ConvertTimeScale(time_offset, timescale, _video_timescale);
	//}

	PacketizerFrameType frame_type = (encoded_frame->_frame_type == FrameType::VideoFrameKey) ? PacketizerFrameType::VideoKeyFrame : PacketizerFrameType::VideoPFrame;

	auto frame_data = std::make_shared<PacketizerFrameData>(
		frame_type,
		encoded_frame->_time_stamp,
		encoded_frame->_duration,
		time_offset,
		_video_track->GetTimeBase(),
		encoded_frame->_buffer);

	AppendVideoFrame(frame_data);

	return true;
}

bool StreamPacketizer::AppendAudioData(std::shared_ptr<EncodedFrame> encoded_frame, uint32_t timescale)
{
	if (_stream_type == PacketizerStreamType::VideoOnly)
	{
		OV_ASSERT2("Since it is video-only, audio data cannot be appended");
		return true;
	}

	if (_audio_track == nullptr)
	{
		OV_ASSERT2(_audio_track != nullptr);
		return false;
	}

	OV_ASSERT2(encoded_frame->_frame_type == FrameType::AudioFrameKey);

	// Converting the timestamp using _audio_timescale if needed
	// if (timescale != _audio_timescale)
	// {
	// 	encoded_frame->_time_stamp = Packetizer::ConvertTimeScale(encoded_frame->_time_stamp, timescale, _audio_timescale);
	// 	encoded_frame->_duration = Packetizer::ConvertTimeScale(encoded_frame->_duration, timescale, _audio_timescale);
	// }

	auto frame_data = std::make_shared<PacketizerFrameData>(
		PacketizerFrameType::AudioFrame,
		encoded_frame->_time_stamp,
		encoded_frame->_duration,
		0,
		_audio_track->GetTimeBase(),
		encoded_frame->_buffer);

	AppendAudioFrame(frame_data);

	return true;
}
