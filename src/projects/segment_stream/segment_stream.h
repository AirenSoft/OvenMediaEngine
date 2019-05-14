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
class SegmentStream : public Stream {
public:
    static std::shared_ptr<SegmentStream>
    Create(const std::shared_ptr<Application> application,
            cfg::PublisherType publisher_type,
            const StreamInfo &info,
            uint32_t worker_count);

    explicit SegmentStream(const std::shared_ptr<Application> application,
                            cfg::PublisherType publisher_type,
                            const StreamInfo &info);

    virtual ~SegmentStream() final;

public :
    void SendVideoFrame(std::shared_ptr<MediaTrack> track,
                        std::unique_ptr<EncodedFrame> encoded_frame,
                        std::unique_ptr<CodecSpecificInfo> codec_info,
                        std::unique_ptr<FragmentationHeader> fragmentation) override;

    void SendAudioFrame(std::shared_ptr<MediaTrack> track,
                        std::unique_ptr<EncodedFrame> encoded_frame,
                        std::unique_ptr<CodecSpecificInfo> codec_info,
                        std::unique_ptr<FragmentationHeader> fragmentation) override;

    bool Start(uint32_t worker_count) override;

    bool Stop() override;

    bool GetPlayList(ov::String &play_list);

    bool GetSegment(const ov::String &file_name, std::shared_ptr<ov::Data> &data);

private :
    std::shared_ptr<StreamPacketyzer> _stream_packetyzer = nullptr;
    std::map<uint32_t, std::shared_ptr<MediaTrack>> _media_tracks;

    time_t _stream_check_time;
    uint32_t _key_frame_interval = 0;
    uint32_t _previous_key_frame_timestamp;
    uint64_t _last_video_timestamp = 0;
    uint64_t _last_audio_timestamp = 0;
    uint64_t _previous_last_video_timestamp = 0;
    uint64_t _previous__last_audio_timestamp = 0;
    uint32_t _video_frame_count;
    uint32_t _audio_frame_count;

};


