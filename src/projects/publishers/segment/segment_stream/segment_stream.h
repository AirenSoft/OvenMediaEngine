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
#include "stream_packetizer.h"
#include <map>

//====================================================================================================
// SegmentStream
//====================================================================================================
class SegmentStream : public pub::Stream
{
public:
    explicit SegmentStream(const std::shared_ptr<pub::Application> application, const info::Stream &info);

    ~SegmentStream();

public :
    void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
    void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

    bool Start(int segment_count, int segment_duration);
    bool Stop() override;

    bool GetPlayList(ov::String &play_list);
	std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name);
    virtual std::shared_ptr<StreamPacketizer> CreateStreamPacketizer(int segment_count,
                                                                    int segment_duration,
                                                                    const  ov::String &segment_prefix,
                                                                    PacketizerStreamType stream_type,
                                                                    std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) = 0;

private :
    std::shared_ptr<StreamPacketizer> _stream_packetizer = nullptr;
    std::map<uint32_t, std::shared_ptr<MediaTrack>> _media_tracks;

    std::shared_ptr<MediaTrack> _video_track;
    std::shared_ptr<MediaTrack> _audio_track;
};
