//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"
#include "base/publisher/stream.h"
#include "stream_packetyzer.h"
#include <map>

//====================================================================================================
// SegmentStream
//====================================================================================================
class SegmentStream : public Stream
{
public:
    explicit SegmentStream(const std::shared_ptr<Application> application, const StreamInfo &info);

    ~SegmentStream();

public :
    void SendVideoFrame(std::shared_ptr<MediaTrack> track,
                        std::unique_ptr<EncodedFrame> encoded_frame,
                        std::unique_ptr<CodecSpecificInfo> codec_info,
                        std::unique_ptr<FragmentationHeader> fragmentation) override;

    void SendAudioFrame(std::shared_ptr<MediaTrack> track,
                        std::unique_ptr<EncodedFrame> encoded_frame,
                        std::unique_ptr<CodecSpecificInfo> codec_info,
                        std::unique_ptr<FragmentationHeader> fragmentation) override;

    bool Start(int segment_count, int segment_duration, uint32_t worker_count);

    bool Stop() override;

    bool GetPlayList(ov::String &play_list);

    bool GetSegment(const ov::String &file_name, std::shared_ptr<ov::Data> &data);

    virtual std::shared_ptr<StreamPacketyzer> CreateStreamPacketyzer(int segment_count,
                                                                    int segment_duration,
                                                                    const  ov::String &segment_prefix,
                                                                    PacketyzerStreamType stream_type,
                                                                    PacketyzerMediaInfo media_info) = 0;

private :
    std::shared_ptr<StreamPacketyzer> _stream_packetyzer = nullptr;
    std::map<uint32_t, std::shared_ptr<MediaTrack>> _media_tracks;
};


