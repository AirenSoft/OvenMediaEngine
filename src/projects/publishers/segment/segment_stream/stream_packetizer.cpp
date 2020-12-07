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

bool StreamPacketizer::AppendVideoData(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_video_track == nullptr)
	{
		OV_ASSERT2("Since it is audio-only, video data cannot be appended");
		return false;
	}

	auto frame_type = (media_packet->GetFlag() == MediaPacketFlag::Key) ? FrameType::VideoFrameKey : FrameType::VideoFrameDelta;
	auto buffer = media_packet->GetData();
	auto duration = media_packet->GetDuration();

	PacketizerFrameType packetizer_frame_type = (frame_type == FrameType::VideoFrameKey) ? PacketizerFrameType::VideoKeyFrame : PacketizerFrameType::VideoPFrame;

	auto frame_data = std::make_shared<PacketizerFrameData>(packetizer_frame_type, media_packet->GetPts(), media_packet->GetDts(), duration, _video_track->GetTimeBase(), buffer);

	AppendVideoFrame(frame_data);

	return true;
}

bool StreamPacketizer::AppendAudioData(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_audio_track == nullptr)
	{
		OV_ASSERT2("Since it is video-only, audio data cannot be appended");
		return false;
	}

	//auto frame_type = (media_packet->GetFlag() == MediaPacketFlag::Key) ? FrameType::AudioFrameKey : FrameType::AudioFrameDelta;
	auto buffer = media_packet->GetData();
	auto duration = media_packet->GetDuration();

	auto frame_data = std::make_shared<PacketizerFrameData>(PacketizerFrameType::AudioFrame, media_packet->GetPts(), media_packet->GetDts(), duration, _audio_track->GetTimeBase(), buffer);

	AppendAudioFrame(frame_data);

	return true;
}
