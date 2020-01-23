//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <deque>
#include <string>
#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/media_route/media_buffer.h>
#include <publishers/segment/segment_stream/packetizer/packetizer.h>

#define DEFAULT_SEGMENT_COUNT (3)
#define DEFAULT_SEGMENT_DURATION (5)

//====================================================================================================
// StreamPacketizer
//====================================================================================================
class StreamPacketizer
{
public:
	StreamPacketizer(const ov::String &app_name, const ov::String &stream_name,
					 int segment_count, int segment_duration,
					 PacketizerStreamType stream_type,
					 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track)
		: _segment_count(segment_count),
		  _segment_duration(segment_duration),
		  _stream_type(stream_type),
		  _video_track(video_track),
		  _audio_track(audio_track)
	{
	}

	virtual ~StreamPacketizer() = default;

public:
	bool AppendVideoData(const std::shared_ptr<MediaPacket> &media_packet, uint32_t timescale, uint64_t time_offset);
	bool AppendAudioData(const std::shared_ptr<MediaPacket> &media_packet, uint32_t timescale);

	// Child must implement this functions
	virtual bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &dEncodedFrameata) = 0;
	virtual bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &data) = 0;
	virtual bool GetPlayList(ov::String &play_list) = 0;
	virtual std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) = 0;

protected:
	std::shared_ptr<Packetizer> _packetizer = nullptr;

	int _segment_count;
	int _segment_duration;

	PacketizerStreamType _stream_type;
	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;
};
