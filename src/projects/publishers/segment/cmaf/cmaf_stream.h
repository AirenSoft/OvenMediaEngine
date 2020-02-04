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
#include "cmaf_stream_packetizer.h"

//====================================================================================================
// CmafStream
//====================================================================================================
class CmafStream : public SegmentStream
{
public:
    static std::shared_ptr<CmafStream> Create(int segment_count,
                                               int segment_duration,
                                               const std::shared_ptr<pub::Application> application,
                                               const info::Stream &info,
                                               uint32_t worker_count,
                                               const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

    CmafStream(const std::shared_ptr<pub::Application> application,
    		const info::Stream &info,
    		const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

	~CmafStream();

    std::shared_ptr<StreamPacketizer> CreateStreamPacketizer(int segment_count,
												int segment_duration,
												const  ov::String &segment_prefix,
												PacketizerStreamType stream_type,
												std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) override
    {
        auto stream_packetizer = std::make_shared<CmafStreamPacketizer>(GetApplication()->GetName(),
                                                                        GetName(),
                                                                        segment_count,
                                                                        segment_duration,
                                                                        segment_prefix,
                                                                        stream_type,
                                                                        video_track, audio_track,
																		_chunked_transfer);

        return std::static_pointer_cast<StreamPacketizer>(stream_packetizer);
    }

private:
	std::shared_ptr<ICmafChunkedTransfer> _chunked_transfer = nullptr;
};
