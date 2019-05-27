//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "stream_packetyzer.h"
#include "segment_stream_private.h"

//====================================================================================================
// Constructor
//  - only dash or hls
//====================================================================================================
StreamPacketyzer::StreamPacketyzer(const ov::String &app_name,
                                   const ov::String &stream_name,
                                   int segment_count,
                                   int segment_duration,
                                   PacketyzerStreamType stream_type,
                                   uint32_t video_timescale,
                                   uint32_t auddio_timescale,
                                   uint32_t video_framerate)
{
    _app_name = app_name;
    _stream_name = stream_name;
    _segment_count = segment_count;
    _segment_duration = segment_duration;
    _video_timescale = video_timescale;
    _audio_timescale = auddio_timescale;
    _video_framerate = (uint32_t) video_framerate;
    _stream_type = stream_type;
    _start_time = time(nullptr);

    _last_video_append_time = _start_time;
    _last_audio_append_time = _start_time;
}

//====================================================================================================
// Destructor
//====================================================================================================
StreamPacketyzer::~StreamPacketyzer()
{
    // Video 데이터 정리
    _video_data_queue.clear();
}

//====================================================================================================
// Append Video Data
//====================================================================================================
bool StreamPacketyzer::AppendVideoData(std::unique_ptr<EncodedFrame> encoded_frame,
                                        uint32_t timescale,
                                        uint64_t time_offset)
{
    if(_stream_type == PacketyzerStreamType::AudioOnly)
        return true;

    // 임시
    timescale = 90000;

    // timestamp change
    if (timescale != _video_timescale)
    {
        encoded_frame->_time_stamp = Packetyzer::ConvertTimeScale(encoded_frame->_time_stamp, timescale, _video_timescale);

        if (time_offset != 0)
            time_offset = Packetyzer::ConvertTimeScale(time_offset, timescale, _video_timescale);
    }

    PacketyzerFrameType frame_type = (encoded_frame->_frame_type == FrameType::VideoFrameKey) ?
                                     PacketyzerFrameType::VideoIFrame : PacketyzerFrameType::VideoPFrame;

    auto frame_data = std::make_shared<PacketyzerFrameData>(frame_type,
                                                            encoded_frame->_time_stamp,
                                                            time_offset,
                                                            _video_timescale,
                                                            encoded_frame->_buffer);

    AppendVideoFrame(frame_data);
    _last_video_timestamp = encoded_frame->_time_stamp;
    _last_video_append_time = time(nullptr);
    return true;
}

//====================================================================================================
// Append Audio Data
//====================================================================================================
bool StreamPacketyzer::AppendAudioData(std::unique_ptr<EncodedFrame> encoded_frame, uint32_t timescale)
{
    if(_stream_type == PacketyzerStreamType::VideoOnly)
        return true;

    // timestamp change
    if (timescale != _audio_timescale)
    {
        encoded_frame->_time_stamp = Packetyzer::ConvertTimeScale(encoded_frame->_time_stamp, timescale, _audio_timescale);
    }

    auto frame_data = std::make_shared<PacketyzerFrameData>(PacketyzerFrameType::AudioFrame,
                                                            encoded_frame->_time_stamp,
                                                            0,
                                                            _audio_timescale,
                                                            encoded_frame->_buffer);

    AppendAudioFrame(frame_data);
    _last_audio_timestamp = encoded_frame->_time_stamp;
    _last_audio_append_time = time(nullptr);
    return true;
}
