//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../segment_stream/stream_packetyzer.h"
#include "dash_packetyzer.h"

//====================================================================================================
// DashStreamPacketyzer
//====================================================================================================
class DashStreamPacketyzer : public StreamPacketyzer
{
public:
    DashStreamPacketyzer(const ov::String &app_name,
                         const ov::String &stream_name,
                         int segment_count,
                        int segment_duration,
                        const  ov::String &segment_prefix,
                        PacketyzerStreamType stream_type,
                        PacketyzerMediaInfo media_info);

    virtual ~DashStreamPacketyzer();

public :

    // Implement StreamPacketyzer Interface
    bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data) override;
    bool GetPlayList(ov::String &play_list) override;
    bool GetSegment(const ov::String &file_name, std::shared_ptr<ov::Data> &data) override;

private :
    //std::shared_ptr<DashPacketyzer> _packetyzer = nullptr;
};

