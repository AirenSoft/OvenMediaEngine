//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "packetyzer.h"
#include <sstream>
#include <algorithm>
#include <sys/time.h>

//====================================================================================================
// Constructor
//====================================================================================================
Packetyzer::Packetyzer(PacketyzerType packetyzer_type,
                       std::string &segment_prefix,
                       PacketyzerStreamType stream_type,
                       uint32_t segment_count,
                       uint32_t segment_duration,
                       PacketyzerMediaInfo &media_info)
{
    _packetyzer_type = packetyzer_type;
    _segment_prefix = segment_prefix;
    _stream_type = stream_type;
    _segment_count = segment_count;
    _segment_save_count = segment_count * 3;
    _segment_duration = segment_duration;

    _media_info = media_info;
    _sequence_number = 1;
    _video_sequence_number = 1;
    _audio_sequence_number = 1;
    _save_file = false;

    _video_init = false;
    _audio_init = false;

    _init_segment_count_complete = false;

    if (_stream_type == PacketyzerStreamType::VideoOnly)_audio_init = true;
    if (_stream_type == PacketyzerStreamType::AudioOnly)_video_init = true;

    _segment_datas.clear();
    _segment_indexer.clear();
    _video_segment_indexer.clear();
    _audio_segment_indexer.clear();
}

//====================================================================================================
// Destructor
//====================================================================================================
Packetyzer::~Packetyzer()
{
    _segment_datas.clear();
    _segment_indexer.clear();
    _video_segment_indexer.clear();
    _audio_segment_indexer.clear();

}

//====================================================================================================
// TimeScale에 따라 시간값(Timestamp) 변경
//====================================================================================================
uint64_t Packetyzer::ConvertTimeScale(uint64_t time, uint32_t from_timescale, uint32_t to_timescale)
{
    if (from_timescale == 0) return 0;

    double ratio = (double) to_timescale / (double) from_timescale;

    return ((uint64_t) ((double) time * ratio));
}


//====================================================================================================
// PlayList
//====================================================================================================
bool Packetyzer::SetPlayList(std::string &play_list)
{
    _play_list = play_list;

    if (_save_file) {
        std::string file_name;

        if (_packetyzer_type == PacketyzerType::Dash) file_name = "manifest.mpd";
        else if (_packetyzer_type == PacketyzerType::Hls) file_name = "playlist.m3u8";

        // init.m4s test 파일 저장
        FILE *file = fopen(file_name.c_str(), "wb");
        fwrite(_play_list.c_str(), 1, _play_list.size(), file);
        fclose(file);
    }
    return true;
}

//====================================================================================================
// Segment
//====================================================================================================
bool Packetyzer::SetSegmentData(SegmentDataType data_type,
                                uint32_t sequence_number,
                                std::string file_name,
                                uint64_t duration,
                                uint64_t timestamp,
                                std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto segment_data = std::make_shared<SegmentData>(sequence_number, file_name, duration, timestamp, data);

    if(file_name == MPD_VIDEO_INIT_FILE_NAME)
    {
        _mpd_video_init_file = segment_data;
        return true;
    }
    else if(file_name == MPD_AUDIO_INIT_FILE_NAME)
    {
        _mpd_audio_init_file = segment_data;
        return true;
    }


    // Segment 저장
    std::unique_lock<std::mutex> segment_datas_lock(_segment_datas_mutex);

    _segment_datas[file_name] = segment_data;

    // 특정 개수 이상 메모리/파일 삭제 처리를 위한 indexer
    // - indexer 등록되지 않은 segment 데이터 유지(ex) init.m4s)
    std::deque<std::string> *indexer = nullptr;

    if (data_type == SegmentDataType::Ts) indexer = &_segment_indexer;
    else if (data_type == SegmentDataType::Mp4Video) indexer = &_video_segment_indexer;
    else if (data_type == SegmentDataType::Mp4Audio) indexer = &_audio_segment_indexer;
    else indexer = &_segment_indexer;

    indexer->push_back(file_name);

    // 데이터 삭제(데이터 전송 요청을 받기 위해 특정 개수 이상 유지)
    while (indexer->size() > _segment_save_count)
    {
        auto segment_data_item = _segment_datas.find(indexer->front());

        // segment data delete
        if (segment_data_item != _segment_datas.end())
        {
            _segment_datas.erase(segment_data_item);
        }

        // file delete
        if (_save_file)
        {
            remove(indexer->front().c_str());
        }

        // segment data indexer delete
        indexer->pop_front();
    }

    segment_datas_lock.unlock();

    if (_save_file)
    {
        FILE *file = file = fopen(file_name.c_str(), "wb");
        fwrite(data->data(), 1, data->size(), file);
        fclose(file);
    }

    if(!_init_segment_count_complete)
    {
        if (data_type == SegmentDataType::Ts && sequence_number >= _segment_count)
        {
            _init_segment_count_complete = true;
        }
        else if ((data_type == SegmentDataType::Mp4Video || data_type == SegmentDataType::Mp4Audio) && 
		sequence_number >= _segment_count*2)
        {
            _init_segment_count_complete = true;
        }
    }

    return true;
}


//====================================================================================================
// PlayList
//====================================================================================================
bool Packetyzer::GetPlayList(std::string &play_list)
{
    if(!_init_segment_count_complete)
        return false;

    play_list = _play_list;
    return true;
}

//====================================================================================================
// Segment
//====================================================================================================
bool Packetyzer::GetSegmentData(std::string &file_name, std::shared_ptr<std::vector<uint8_t>> &data)
{
	if(!_init_segment_count_complete)
        return false;

    if(file_name == MPD_VIDEO_INIT_FILE_NAME && _mpd_video_init_file != nullptr)
    {
        data =_mpd_video_init_file->data;
        return true;
    }
    else if(file_name == MPD_AUDIO_INIT_FILE_NAME && _mpd_audio_init_file != nullptr)
    {
        data =_mpd_audio_init_file->data;
        return true;
    }

    std::unique_lock<std::mutex> segment_datas_lock(_segment_datas_mutex);

    auto segment_data_item = _segment_datas.find(file_name);

    if (segment_data_item != _segment_datas.end())
    {
        data = segment_data_item->second->data;
        return true;
    }
    return false;
}

//====================================================================================================
// Gcd(Util)
//====================================================================================================
uint32_t Packetyzer::Gcd(uint32_t n1, uint32_t n2)
{
    uint32_t temp;

    while (n2 != 0)
    {
        temp = n1;
        n1 = n2;
        n2 = temp % n2;
    }
    return n1;
}

//====================================================================================================
// Packetyzer(Util)
// - 1/1000
//====================================================================================================
double Packetyzer::GetCurrentMilliseconds()
{
    struct timeval time_value;
    gettimeofday(&time_value, nullptr); // get current time
    double milliseconds = time_value.tv_sec*1000LL + time_value.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}

//====================================================================================================
// MakeUtcTimeString
// - ex)
//====================================================================================================
std::string Packetyzer::MakeUtcTimeString(time_t value)
{
    std::tm *now_tm = gmtime(&value);
    char buf[42];
    strftime(buf, 42, "\"%Y-%m-%dT%TZ\"", now_tm);
    return buf;
}
