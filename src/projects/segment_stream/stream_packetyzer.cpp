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

#define MAX_INPUT_DATA_SIZE            (10485760*2) // 20mb

//====================================================================================================
// Constructor
//  - only dash or hls
//====================================================================================================
StreamPacketyzer::StreamPacketyzer(PacketyzerStreamType stream_type,
                                    uint32_t video_timescale,
                                    uint32_t auddio_timescale,
                                    uint32_t video_framerate)
{
    _video_timescale = video_timescale;
    _audio_timescale = auddio_timescale;
    _video_framerate = (uint32_t) video_framerate;
    _stream_type = stream_type;
    _start_time = time(nullptr);

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
bool StreamPacketyzer::AppendVideoData(uint64_t timestamp,
                                        uint32_t timescale,
                                        bool is_keyframe,
                                        uint64_t time_offset,
                                        uint32_t data_size,
                                        const uint8_t *data)
{
    // 임시
    timescale = 90000;

    // data valid check
    if (data_size <= 0 || data_size > MAX_INPUT_DATA_SIZE)
    {
        logtw("Data Size Error(%d:%d)", data_size, MAX_INPUT_DATA_SIZE);
        return false;
    }



    // timestamp change
    if (timescale != _video_timescale)
    {
        timestamp = Packetyzer::ConvertTimeScale(timestamp, timescale, _video_timescale);
        if (time_offset != 0) time_offset = Packetyzer::ConvertTimeScale(time_offset, timescale, _video_timescale);
    }

    auto frame = std::make_shared<std::vector<uint8_t>>(data, data + data_size);
    auto video_data = std::make_shared<PacketyzerFrameData>(
            is_keyframe ? PacketyzerFrameType::VideoIFrame : PacketyzerFrameType::VideoPFrame,
            timestamp,
            time_offset,
            _video_timescale,
            frame);

    // Video data save
    if (_stream_type == PacketyzerStreamType::VideoOnly)
    {
        AppendVideoFrame(video_data);
        _last_video_timestamp = timestamp;
    }
    else
    {
        _video_data_queue.push_back(video_data);
    }

    return true;
}

//====================================================================================================
// Video Data SampleWrite
// - audio 기준시간 보다 0.5초 이상 크면 저장 데이터의 마지막까지 써준다.
//====================================================================================================
bool StreamPacketyzer::VideoDataSampleWrite(uint64_t timestamp)
{
    bool fource_append = false;

    // none data
   if(_video_data_queue.empty())
        return true;

    // Queue size over check( Framerate)
    if (_video_data_queue.size() > _video_framerate)
    {
        // 첫 프레임 부터 오디오 기준 시간 보다 크면 모두 써준다.
        // - 이미 sync 처리하기는 힘든 상황
        // - 이처리가 없으면 I프레임 대기 상태로 Segment를 만들지 못함
        if (_video_data_queue.front()->timestamp > timestamp + _video_timescale)
        {
            fource_append = true;
        }
        else
        {
            int delete_count = 0;

            while (!_video_data_queue.empty())
            {
                auto video_data = _video_data_queue.front();

                if (video_data->timestamp <= timestamp)
                {
                    break;
                }
//            logtd("VideoDataQueue Delete Frame - Timestamp(%lldms/%lldms)",
//                    video_data->timestamp/(_video_timescale/1000), timestamp/(_video_timescale/1000));

                delete_count++;
                _video_data_queue.pop_front();
            }

            logtd("VideoDataQueue Count Over - Count(%d:%d) Timestamp(%lldms) Duration(%d) Delete(%d)",
                  (int) _video_data_queue.size(),
                  _video_framerate,
                  timestamp / (_video_timescale / 1000),
                  time(nullptr) - _start_time,
                  delete_count);
        }
    }

    //Video data append
    while (!_video_data_queue.empty())
    {
        auto video_data = _video_data_queue.front();

        if (video_data != nullptr)
        {
            if ((video_data->timestamp > timestamp) && !fource_append)
            {
                break;
            }

            AppendVideoFrame(video_data);

            _last_video_timestamp = timestamp;

        }

        _video_data_queue.pop_front();
    }

    return true;
}

//====================================================================================================
// Append Audio Data
//====================================================================================================
bool StreamPacketyzer::AppendAudioData(uint64_t timestamp,
                                        uint32_t timescale,
                                        uint32_t data_size,
                                        const uint8_t *data)
{
    // data valid check
    if (data_size <= 0 || data_size > MAX_INPUT_DATA_SIZE)
    {
        logtw("Data Size Error(%d:%d)", data_size, MAX_INPUT_DATA_SIZE);
        return false;
    }

    // Video 데이터 삽입(오디오 타임스탬프 이전 데이터)
    VideoDataSampleWrite(Packetyzer::ConvertTimeScale(timestamp, timescale, _video_timescale));

    // timestamp change
    if (timescale != _audio_timescale)
    {
        timestamp = Packetyzer::ConvertTimeScale(timestamp, timescale, _audio_timescale);
    }

    auto frame = std::make_shared<std::vector<uint8_t>>(data, data + data_size);
    auto audio_data = std::make_shared<PacketyzerFrameData>(PacketyzerFrameType::AudioFrame,
                                                            timestamp,
                                                            0,
                                                            _audio_timescale,
                                                            frame);

     // Audio 데이터 삽입
    AppendAudioFrame(audio_data);

    _last_audio_timestamp = timestamp;

    return true;
}
