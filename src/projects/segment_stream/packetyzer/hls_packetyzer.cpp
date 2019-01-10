//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_packetyzer.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <array>
#include <algorithm>

#define HLS_MAX_TEMP_VIDEO_DATA_COUNT        (500)

//====================================================================================================
// Constructor
//====================================================================================================
HlsPacketyzer::HlsPacketyzer(std::string &segment_prefix,
                             PacketyzerStreamType stream_type,
                             uint32_t segment_count,
                             uint32_t segment_duration,
                             PacketyzerMediaInfo &media_info) :
        Packetyzer(PacketyzerType::Hls, segment_prefix, stream_type, segment_count, (uint32_t) segment_duration,
                   media_info)
{
    _media_info.audio_timescale = PACKTYZER_DEFAULT_TIMESCALE;
}

//====================================================================================================
// Video Frame
//====================================================================================================
bool HlsPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    // 스트림 타입 확인
    if (_stream_type == PacketyzerStreamType::AudioOnly)
    {
        return true;
    }

    // Init
    if (!_video_init)
    {
        // 첫 Key 프레임 대기
        if (frame_data->type != PacketyzerFrameType::VideoIFrame)
        {
            return true;
        }

        _video_init = true;
    }

    uint64_t timestamp = frame_data->timestamp;
    uint64_t time_offset = frame_data->time_offset;

    if (frame_data->timescale != _media_info.video_timescale)
    {
        timestamp = ConvertTimeScale(frame_data->timestamp, frame_data->timescale, _media_info.video_timescale);
        time_offset = ConvertTimeScale(frame_data->time_offset, frame_data->timescale, _media_info.video_timescale);
    }

    if (frame_data->type == PacketyzerFrameType::VideoIFrame && !_frame_datas.empty())
    {
        // Duration Check
        uint64_t duration = timestamp - _frame_datas[0]->timestamp;

        // I-Frame 관련 threshold 87% 한도 내에서 처리
        if (duration > (double) _segment_duration * 0.87 * _media_info.video_timescale)
        {
            // Segment Write
            SegmentWrite(_frame_datas[0]->timestamp, duration);
        }
    }

    // 데이터 복사
    auto frame = std::make_shared<std::vector<uint8_t>>(frame_data->data->data(),
                                                        frame_data->data->data() + frame_data->data->size());
    auto video_data = std::make_shared<PacketyzerFrameData>(frame_data->type, timestamp, time_offset,
                                                            PACKTYZER_DEFAULT_TIMESCALE, frame);

    _frame_datas.push_back(video_data);

    return true;

}

//====================================================================================================
// Video Frame
//====================================================================================================
bool HlsPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (_stream_type == PacketyzerStreamType::VideoOnly)
    {
        return true;
    }

    // 비디오 초기화 확인(Key Frame 확인)
    if (!_video_init)
    {
        return true;
    }

    if (!_audio_init)
    {
        // 초기 필요 작업
        _audio_init = true;
    }

    uint64_t timestamp = (frame_data->timescale == _media_info.audio_timescale) ? frame_data->timestamp
                                                                                : ConvertTimeScale(
                    frame_data->timestamp, frame_data->timescale, _media_info.audio_timescale);

    if (_stream_type == PacketyzerStreamType::AudioOnly && !_frame_datas.empty())
    {
        // Duration Check
        uint64_t duration = timestamp - _frame_datas[0]->timestamp;

        if (duration >= (_segment_duration * _media_info.audio_timescale))
        {
            // Segment Write
            SegmentWrite(_frame_datas[0]->timestamp, duration);
        }
    }

    // 데이터 복사
    auto frame = std::make_shared<std::vector<uint8_t>>(frame_data->data->data(),
                                                        frame_data->data->data() + frame_data->data->size());
    auto audio_data = std::make_shared<PacketyzerFrameData>(frame_data->type, timestamp, 0, _media_info.audio_timescale,
                                                            frame);

    _frame_datas.push_back(audio_data);

    return true;
}

//====================================================================================================
// Segment Write
//====================================================================================================
bool HlsPacketyzer::SegmentWrite(uint64_t start_timestamp, uint64_t duration)
{
    auto ts_writer = std::make_unique<TsWriter>(_stream_type);

    for (auto &frame_data : _frame_datas)
    {
        //if (frame_data->type == PacketyzerFrameType::AudioFrame) continue;

        // TS(PES) Write
        ts_writer->WriteSample(frame_data->type != PacketyzerFrameType::AudioFrame,
                               frame_data->type == PacketyzerFrameType::AudioFrame ||
                               frame_data->type == PacketyzerFrameType::VideoIFrame,
                               frame_data->timestamp,
                               frame_data->time_offset,
                               frame_data->data);
    }

    std::ostringstream file_name_stream;
    file_name_stream << _segment_prefix << "_" << _sequence_number << ".ts";

    SetSegmentData(SegmentDataType::Ts, _sequence_number, file_name_stream.str(), duration, start_timestamp,
                   ts_writer->GetDataStream(), true);

    UpdatePlayList();

    _sequence_number++;

    _frame_datas.clear();

    return true;
}

//====================================================================================================
// PlayList(M3U8) 업데이트 
// 방송번호_인덱스.TS
//====================================================================================================
bool HlsPacketyzer::UpdatePlayList()
{
    std::ostringstream play_list;
    std::ostringstream m3u8_play_list;
    double max_duration = 0;

    std::unique_lock<std::mutex> segment_datas_lock(_segment_datas_mutex);

    for (int index = std::max(0, (int) (_segment_indexer.size() - _segment_count));
         index < _segment_indexer.size(); index++)
    {
        auto file_name = _segment_indexer[index];
        auto item = _segment_datas.find(file_name);

        if (item != _segment_datas.end())
        {
            auto segment_data = item->second;

            m3u8_play_list << "#EXTINF:" << std::fixed << std::setprecision(3)
                           << segment_data->duration / PACKTYZER_DEFAULT_TIMESCALE << ",\r\n"
                           << file_name << "\r\n";

            if (segment_data->duration > max_duration)
            {
                max_duration = segment_data->duration;
            }
        }
    }
    segment_datas_lock.unlock();

    play_list << "#EXTM3U" << "\r\n"
              << "#EXT-X-MEDIA-SEQUENCE:" << _sequence_number + 1 << "\r\n"
              << "#EXT-X-VERSION:3" << "\r\n"
              << "#EXT-X-ALLOW-CACHE:NO" << "\r\n"
              << "#EXT-X-TARGETDURATION:" << (int) (max_duration / PACKTYZER_DEFAULT_TIMESCALE) << "\r\n"
              << m3u8_play_list.str();

    // Playlist 설정
    std::string play_list_data = play_list.str();
    SetPlayList(play_list_data);

    return true;
}
