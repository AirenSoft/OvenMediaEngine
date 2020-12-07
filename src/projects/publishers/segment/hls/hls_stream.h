//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <publishers/segment/segment_stream/segment_stream.h>

#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "hls_stream_packetizer.h"

class HlsStream : public SegmentStream
{
public:
	static std::shared_ptr<HlsStream> Create(int segment_count, int segment_duration,
											 const std::shared_ptr<pub::Application> &application,
											 const info::Stream &info,
											 uint32_t worker_count);

	HlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info);
	~HlsStream() override;

	std::shared_ptr<StreamPacketizer> CreateStreamPacketizer(int segment_count, int segment_duration,
															 const ov::String &segment_prefix,
															 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) override
	{
		auto stream_packetizer = std::make_shared<HlsStreamPacketizer>(GetApplicationName(), GetName(),
																	   segment_count, segment_duration,
																	   segment_prefix,
																	   video_track, audio_track);

		return std::static_pointer_cast<StreamPacketizer>(stream_packetizer);
	}
};
