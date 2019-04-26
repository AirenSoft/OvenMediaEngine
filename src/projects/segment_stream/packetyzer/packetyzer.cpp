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


    // init nullptr
    for(int index = 0; index < _segment_save_count ;  index++)
    {
        _video_segment_datas.push_back(nullptr);

        // only dash
        if(_packetyzer_type == PacketyzerType::Dash)
            _audio_segment_datas.push_back(nullptr);
    }



}

//====================================================================================================
// Destructor
//====================================================================================================
Packetyzer::~Packetyzer()
{

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
// - thread safe
//====================================================================================================
bool Packetyzer::SetPlayList(std::string &play_list)
{
    // playlist mutex
    std::unique_lock<std::mutex> lock(_play_list_guard);

    _play_list = play_list;

    if (_save_file)
    {
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
// - thread safe
//====================================================================================================
bool Packetyzer::SetSegmentData(SegmentDataType data_type,
                                uint32_t sequence_number,
                                std::string file_name,
                                uint64_t duration,
                                uint64_t timestamp,
                                const std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto segment_data = std::make_shared<SegmentData>(sequence_number,
                                                      file_name.c_str(),
                                                      duration,
                                                      timestamp,
                                                      data->size(),
                                                      data->data());

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

    if (data_type == SegmentDataType::Ts || data_type == SegmentDataType::Mp4Video)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        _video_segment_datas[_current_video_index++] = segment_data;

        if(_segment_save_count <= _current_video_index)
            _current_video_index = 0;
    }
    else if (data_type == SegmentDataType::Mp4Audio)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        _audio_segment_datas[_current_audio_index++] = segment_data;

        if(_segment_save_count <= _current_audio_index)
            _current_audio_index = 0;
    }

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
// - thread safe
//====================================================================================================
bool Packetyzer::GetPlayList(std::string &play_list)
{
    if(!_init_segment_count_complete)
        return false;

    // playlist mutex
    std::unique_lock<std::mutex> lock(_play_list_guard);

    play_list = _play_list;

    return true;
}

//====================================================================================================
// Segment
// - thread safe
//====================================================================================================
bool Packetyzer::GetSegmentData(SegmentDataType data_type,
                                const ov::String &file_name,
                                std::shared_ptr<ov::Data> &data)
{
	if(!_init_segment_count_complete)
        return false;

	// mpd init file
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

    // ts or mpd data
    if (data_type == SegmentDataType::Ts || data_type == SegmentDataType::Mp4Video)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        auto item = std::find_if(_video_segment_datas.begin(), _video_segment_datas.end(), [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

        if(item != _video_segment_datas.end())
            data = (*item)->data;
        else
            data = nullptr;
    }
    else if (data_type == SegmentDataType::Mp4Audio)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        auto item = std::find_if(_audio_segment_datas.begin(), _audio_segment_datas.end(), [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

        if(item != _audio_segment_datas.end())
            data = (*item)->data;
        else
            data = nullptr;
    }

    return data != nullptr;

    // TODO: search
    // - (current index) == (last insert index + 1)
    // - form (current index - 1) it reverse search  to (begin it)
    // - form (end it) reverse to (current index)
}



//====================================================================================================
// Last (segment count) Video(or Video+Audio) Segments
// - thread safe
//====================================================================================================
bool Packetyzer::GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas)
{
    int begin_index = (_current_video_index >= _segment_count) ?
                        (_current_video_index - _segment_count) :
                        (_segment_save_count - (_segment_count - _current_video_index));


    int end_index = (begin_index <= (_segment_save_count - _segment_count)) ?
                    (begin_index + _segment_count) -1 :
                    (_segment_count - (_segment_save_count - begin_index)) -1;

    // video segment mutex
    std::unique_lock<std::mutex> lock(_video_segment_guard);

    if(begin_index <= end_index)
    {
        for(auto item = _video_segment_datas.begin() + begin_index;item <= _video_segment_datas.begin() + end_index; item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }
    }
    else
    {
        for(auto item = _video_segment_datas.begin() + begin_index;item < _video_segment_datas.end(); item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }

        for (auto item = _video_segment_datas.begin(); item <= _video_segment_datas.begin() + end_index; item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }
    }

    return true;
}

//====================================================================================================
// Last (segment count) Audio Segments
// - thread safe
//====================================================================================================
bool Packetyzer::GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas)
{
    int begin_index = (_current_audio_index >= _segment_count) ?
                      (_current_audio_index - _segment_count) :
                      (_segment_save_count - (_segment_count - _current_audio_index));


    int end_index = (begin_index <= (_segment_save_count - _segment_count)) ?
                    (begin_index + _segment_count) -1 :
                    (_segment_count - (_segment_save_count - begin_index)) -1;

    // audio segment mutex
    std::unique_lock<std::mutex> lock(_audio_segment_guard);

    if(begin_index <= end_index)
    {
        for(auto item = _audio_segment_datas.begin() + begin_index;item <= _audio_segment_datas.begin() + end_index; item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }
    }
    else
    {
        for(auto item = _audio_segment_datas.begin() + begin_index;item < _audio_segment_datas.end(); item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }
        for (auto item = _audio_segment_datas.begin(); item <= _audio_segment_datas.begin() + end_index; item++)
        {
            if(*item == nullptr)
                return true;

            segment_datas.push_back(*item);
        }
    }

    return true;
}