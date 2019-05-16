//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream/stream_packetyzer.h"
#include "segment_stream/packetyzer/hls_packetyzer.h"

//====================================================================================================
// HlsStreamPacketyzer
//====================================================================================================
class HlsStreamPacketyzer : public StreamPacketyzer
{
public:
    HlsStreamPacketyzer(int segment_count,
                         int segment_duration,
                         std::string &segment_prefix,
                         PacketyzerStreamType stream_type,
                         PacketyzerMediaInfo media_info);

    virtual ~HlsStreamPacketyzer();

public :

    // Implement StreamPacketyzer Interface
    bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool GetPlayList(ov::String &segment_play_list) override;
    bool GetSegment(const ov::String &file_name, std::shared_ptr<ov::Data> &data) override;

private :
    std::shared_ptr<HlsPacketyzer> _packetyzer = nullptr;
};

