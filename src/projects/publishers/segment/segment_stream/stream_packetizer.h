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
#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>
#include <publishers/segment/segment_stream/packetizer/packetizer.h>

#include <deque>
#include <string>

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
					 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
					 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
		: _segment_count(segment_count),
		  _segment_duration(segment_duration),
		  _video_track(video_track),
		  _audio_track(audio_track),

		  _chunked_transfer(chunked_transfer)
	{
	}

	virtual ~StreamPacketizer() = default;

public:
	virtual bool AppendVideoData(const std::shared_ptr<MediaPacket> &media_packet);
	virtual bool AppendAudioData(const std::shared_ptr<MediaPacket> &media_packet);

	// Child must implement this functions
	virtual bool AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &dEncodedFrameata) = 0;
	virtual bool AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &data) = 0;

	virtual bool GetPlayList(ov::String &play_list) = 0;
	virtual std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const = 0;

protected:
	std::shared_ptr<Packetizer> _packetizer = nullptr;

	int _segment_count;
	int _segment_duration;

	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;

	std::shared_ptr<ChunkedTransferInterface> _chunked_transfer;
};
