//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <publishers/segment/segment_stream/stream_packetizer.h>

#include "dash_packetizer.h"

class DashStreamPacketizer : public StreamPacketizer
{
public:
	DashStreamPacketizer(const ov::String &app_name, const ov::String &stream_name,
						 int segment_count, int segment_duration,
						 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
						 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

public:
	bool AppendVideoData(const std::shared_ptr<MediaPacket> &media_packet) override;
	bool AppendAudioData(const std::shared_ptr<MediaPacket> &media_packet) override;

	bool AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &data) override
	{
		return false;
	}

	bool AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &data) override
	{
		return false;
	}

	bool GetPlayList(ov::String &play_list) override;
	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const override;
};
