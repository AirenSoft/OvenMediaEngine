//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <deque>
#include <string>
#include "packetyzer/packetyzer.h"

#define DEFAULT_SEGMENT_COUNT (3)
#define DEFAULT_SEGMENT_DURATION (5)

//====================================================================================================
// StreamPacketyzer
//====================================================================================================
class StreamPacketyzer
{
public:
	StreamPacketyzer(const ov::String &app_name, const ov::String &stream_name,
					 int segment_count, int segment_duration,
					 PacketyzerStreamType stream_type,
					 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track)
		: _segment_count(segment_count),
		  _segment_duration(segment_duration),
		  _video_track(video_track),
		  _audio_track(audio_track),
		  _stream_type(stream_type)
	{
	}

	virtual ~StreamPacketyzer() = default;

public:
	bool AppendVideoData(std::unique_ptr<EncodedFrame> encoded_frame, uint32_t timescale, uint64_t time_offset);
	bool AppendAudioData(std::unique_ptr<EncodedFrame> encoded_frame, uint32_t timescale);

	// Child must implement this functions
	virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &dEncodedFrameata) = 0;
	virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data) = 0;
	virtual bool GetPlayList(ov::String &play_list) = 0;
	virtual std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) = 0;

protected:
	std::shared_ptr<Packetyzer> _packetyzer = nullptr;

	int _segment_count;
	int _segment_duration;

	PacketyzerStreamType _stream_type;
	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;
};
