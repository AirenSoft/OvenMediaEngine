//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../segment_stream/stream_packetyzer.h"
#include "cmaf_packetyzer.h"

class CmafStreamPacketyzer : public StreamPacketyzer
{
public:
    CmafStreamPacketyzer(const ov::String &app_name,
						const ov::String &stream_name,
						int segment_count,
						int segment_duration,
						const  ov::String &segment_prefix,
						PacketyzerStreamType stream_type,
						std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
						const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

    virtual ~CmafStreamPacketyzer();

public :

    // Implement StreamPacketyzer Interface
    bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool GetPlayList(ov::String &play_list) override;
	std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;

private :

};

