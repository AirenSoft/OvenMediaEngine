//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "hls_stream_packetyzer.h"
#include "segment_stream/segment_stream.h"

//====================================================================================================
// HlsStream
//====================================================================================================
class HlsStream : public SegmentStream
{
public:
	static std::shared_ptr<HlsStream> Create(int segment_count,
											 int segment_duration,
											 const std::shared_ptr<Application> application,
											 const StreamInfo &info,
											 uint32_t worker_count);

	HlsStream(const std::shared_ptr<Application> application, const StreamInfo &info);

	~HlsStream();

	std::shared_ptr<StreamPacketyzer> CreateStreamPacketyzer(int segment_count,
															 int segment_duration,
															 const ov::String &segment_prefix,
															 PacketyzerStreamType stream_type,
															 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) override
	{
		auto stream_packetyzer = std::make_shared<HlsStreamPacketyzer>(GetApplication()->GetName(),
																	   GetName(),
																	   segment_count,
																	   segment_duration,
																	   segment_prefix,
																	   stream_type,
																	   video_track, audio_track);

		return std::static_pointer_cast<StreamPacketyzer>(stream_packetyzer);
	}

private:
};
