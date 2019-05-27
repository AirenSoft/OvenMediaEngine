//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_packetyzer.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <numeric>
#include "dash_private.h"

#define VIDEO_TRACK_ID    (1)
#define AUDIO_TRACK_ID    (2)

//====================================================================================================
// Constructor
//====================================================================================================
DashPacketyzer::DashPacketyzer(const ov::String &app_name,
                            const ov::String &stream_name,
                            PacketyzerStreamType stream_type,
                            const ov::String &segment_prefix,
                            uint32_t segment_count,
                            uint32_t segment_duration,
                            PacketyzerMediaInfo &media_info) :
                                Packetyzer(app_name,
                                            stream_name,
                                            PacketyzerType::Dash,
                                            stream_type,
                                            segment_prefix,
                                            segment_count,
                                            segment_duration,
                                            media_info)
{
    _avc_nal_header_size = 0;
    _video_frame_datas.clear();
    _audio_frame_datas.clear();

    uint32_t resolution_gcd = Gcd(media_info.video_width, media_info.video_height);
    if (resolution_gcd != 0)
    {
        std::ostringstream pixel_aspect_ratio;
        pixel_aspect_ratio << media_info.video_width / resolution_gcd << ":"
                           << media_info.video_height / resolution_gcd;
        _mpd_pixel_aspect_ratio = pixel_aspect_ratio.str();
    }

    _mpd_suggested_presentation_delay = (_segment_count / 2 * _segment_duration);
    _mpd_min_buffer_time = (_segment_duration*1.2);
    _mpd_update_time = GetCurrentMilliseconds();

    _last_video_append_time = time(nullptr);
    _last_audio_append_time = time(nullptr);

    _duration_threshold = (double)_segment_duration * 0.9 * (double)_media_info.video_timescale;
}

//====================================================================================================
// Destructor
//====================================================================================================
DashPacketyzer::~DashPacketyzer()
{
    _video_frame_datas.clear();
    _audio_frame_datas.clear();
}

//====================================================================================================
// Video 설정 값 Load ( Key Frame 데이터만 가능)
//  0001 + sps + 0001 + pps + 0001 + I-Frame 구조 파싱
// - profile
// - level
// - sps/pps
// - init m4s 생성
//====================================================================================================
bool DashPacketyzer::VideoInit(std::shared_ptr<ov::Data> &frame_data)
{
    // 패턴 확인
    uint32_t current_index = 0;
    int sps_start_index = -1;
    int sps_end_index = -1;
    int pps_start_index = -1;
    int pps_end_index = -1;
    auto data = std::make_shared<std::vector<uint8_t>>(frame_data->GetDataAs<uint8_t>(),
            frame_data->GetDataAs<uint8_t>() + frame_data->GetLength());

    // sps/pps parsing
    while ((current_index + AVC_NAL_START_PATTERN_SIZE) < data->size())
    {
        // 0x00 0x00 0x00 0x01 패턴 체크
        if (data->at(current_index) == 0 &&
            data->at(current_index + 1) == 0 &&
            data->at(current_index + 2) == 0 &&
            data->at(current_index + 3) == 1)
        {
            if (sps_start_index == -1)
            {
                sps_start_index = current_index + AVC_NAL_START_PATTERN_SIZE;
                current_index += AVC_NAL_START_PATTERN_SIZE;
                continue;
            }
            else if (sps_end_index == -1)
            {
                sps_end_index = current_index - 1;
                pps_start_index = current_index + AVC_NAL_START_PATTERN_SIZE;
                current_index += AVC_NAL_START_PATTERN_SIZE;
                continue;
            }
            else if (pps_end_index == -1)
            {
                pps_end_index = current_index - 1;
                current_index++;
                continue;
            }
        }
        // 0x00 0x00 0x01 패턴 체크
        else if (data->at(current_index) == 0 &&
                data->at(current_index + 1) == 0 &&
                data->at(current_index + 2) == 1)
        {
            if (sps_start_index == -1)
            {
                sps_start_index = current_index + (AVC_NAL_START_PATTERN_SIZE - 1);
                current_index += (AVC_NAL_START_PATTERN_SIZE - 1);
                continue;
            }
            else if (sps_end_index == -1)
            {
                sps_end_index = current_index - 1;
                pps_start_index = current_index + (AVC_NAL_START_PATTERN_SIZE - 1);
                current_index += (AVC_NAL_START_PATTERN_SIZE - 1);
                continue;
            }
            else if (pps_end_index == -1)
            {
                pps_end_index = current_index - 1;
                current_index++;
                continue;
            }
        }
        current_index++;
    }

    // parsing result check
    if (sps_start_index < 0 || sps_end_index < 0 || pps_start_index < 0 || pps_end_index < 0)
    {
        return false;
    }

    // SPS/PPS 저장
    auto avc_sps = std::make_shared<std::vector<uint8_t>>(data->begin() + sps_start_index,
                                                          data->begin() + sps_end_index + 1);

    auto avc_pps = std::make_shared<std::vector<uint8_t>>(data->begin() + pps_start_index,
                                                          data->begin() + pps_end_index + 1);

    // Video init m4s 생성(메모리)
    auto writer = std::make_unique<M4sInitWriter>(M4sMediaType::VideoMediaType,
                                                  1024,
                                                  _segment_duration * _media_info.video_timescale,
                                                  _media_info.video_timescale,
                                                  VIDEO_TRACK_ID,
                                                  _media_info.video_width,
                                                  _media_info.video_height,
                                                  avc_sps,
                                                  avc_pps,
                                                  _media_info.audio_channels,
                                                  16,
                                                  _media_info.audio_samplerate);

    if (writer->CreateData() <= 0)
    {
        logte("video writer create fail");
        return false;
    }

    _avc_nal_header_size = avc_sps->size() + avc_pps->size() + AVC_NAL_START_PATTERN_SIZE * 3;

    // Video init m4s Save
    SetSegmentData(MPD_VIDEO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());

    _video_init = true;

    return true;
}

//====================================================================================================
// Audio 설정 값 Load
// - sample rate
// - channels
// - init m4s 생성
//====================================================================================================
bool DashPacketyzer::AudioInit()
{
    std::shared_ptr<std::vector<uint8_t>> temp = nullptr;

    // Audio init m4s 생성(메모리)
    auto writer = std::make_unique<M4sInitWriter>(M4sMediaType::AudioMediaType,
                                                  1024,
                                                  _segment_duration * _media_info.audio_timescale,
                                                  _media_info.audio_timescale,
                                                  AUDIO_TRACK_ID,
                                                  _media_info.video_width,
                                                  _media_info.video_height,
                                                  temp,
                                                  temp,
                                                  _media_info.audio_channels,
                                                  16,
                                                  _media_info.audio_samplerate);

    if (writer->CreateData() <= 0)
    {
        logte("Audio writer create fail");
        return false;
    }

    // Audio init m4s Save
    SetSegmentData(MPD_AUDIO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());

    _audio_init = true;

    return true;
}

//====================================================================================================
// Video Frame Append
// - Video(Audio) Segment m4s Create
//====================================================================================================
bool DashPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (!_video_init)
    {
        if (frame_data->type != PacketyzerFrameType::VideoIFrame)
            return false;

        if(!VideoInit(frame_data->data))
            return false;
    }

    // Fragment Check
    // - KeyFrame ~ KeyFrame 전까지
    if (frame_data->type == PacketyzerFrameType::VideoIFrame && !_video_frame_datas.empty())
    {
        if (frame_data->timestamp - _video_frame_datas[0]->timestamp >=
                (_segment_duration * _media_info.video_timescale))
        {
            VideoSegmentWrite(frame_data->timestamp);

            if (!_audio_frame_datas.empty())
            {
                AudioSegmentWrite(ConvertTimeScale(frame_data->timestamp,
                                                    _media_info.video_timescale,
                                                    _media_info.audio_timescale), false);
            }

            UpdatePlayList();
        }
    }

    _video_frame_datas.push_back(frame_data);
    _last_video_append_time = time(nullptr);

    return true;
}

//====================================================================================================
// Audio Frame Append
// - Audio Segment m4s Create
//====================================================================================================
bool DashPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (!_audio_init && !AudioInit())
    {
        return false;
    }

    // audio only or video input error
    if ((time(nullptr) - _last_video_append_time >=
            static_cast<uint32_t>(_segment_duration)) && !_audio_frame_datas.empty())
    {
        if ((frame_data->timestamp - _audio_frame_datas[0]->timestamp) >=
                (_segment_duration * _media_info.audio_timescale))
        {
            AudioSegmentWrite(frame_data->timestamp, true);

            UpdatePlayList();
        }
    }

    _audio_frame_datas.push_back(frame_data);
    _last_audio_append_time = time(nullptr);

    return true;
}

//====================================================================================================
// Video Segment Write
// - Duration/Key Frame 확인 이후 이전 데이터 까지 생성
//====================================================================================================
bool DashPacketyzer::VideoSegmentWrite(uint64_t max_timestamp)
{
    uint64_t start_timestamp = 0;
    std::vector<std::shared_ptr<FragmentSampleData>> sample_datas;

    while (!_video_frame_datas.empty())
    {
        auto frame_data = _video_frame_datas.front();

        if (start_timestamp == 0)
            start_timestamp = frame_data->timestamp;

        if (frame_data->timestamp >= max_timestamp)
            break;

        _video_frame_datas.pop_front();

        uint64_t duration = _video_frame_datas.empty() ? (max_timestamp - frame_data->timestamp) :
                            (_video_frame_datas.front()->timestamp - frame_data->timestamp);

        int offset = (frame_data->type == PacketyzerFrameType::VideoIFrame) ?
                                        _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE;

        frame_data->data = frame_data->data->Subdata(offset);

        auto sample_data = std::make_shared<FragmentSampleData>(duration,
                                                                frame_data->type == PacketyzerFrameType::VideoIFrame
                                                                ? 0X02000000 : 0X01010000,
                                                                frame_data->time_offset,
                                                                frame_data->data);

        sample_datas.push_back(sample_data);
    }
    _video_frame_datas.clear();


    // Fragment 쓰기
    auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::VideoMediaType,
                                                                4096,
                                                                _video_sequence_number,
                                                                VIDEO_TRACK_ID,
                                                                start_timestamp,
                                                                sample_datas);

    fragment_writer->CreateData();

    std::ostringstream file_name_stream;
    file_name_stream << _segment_prefix.CStr() << "_" << start_timestamp << MPD_VIDEO_SUFFIX;

    ov::String file_name = file_name_stream.str().c_str();

    // m4s 데이터 저장
    SetSegmentData(file_name, max_timestamp - start_timestamp, start_timestamp, fragment_writer->GetDataStream());

    return true;
}

//====================================================================================================
// Audio Segment Write
// - 비디오 Segment 생성이후 생성 or Audio Only 에서 생성
//====================================================================================================
bool DashPacketyzer::AudioSegmentWrite(uint64_t max_timestamp, bool is_audio_timestamp)
{
    uint64_t start_timestamp = 0;
    uint64_t end_timestamp = 0;
    std::vector<std::shared_ptr<FragmentSampleData>> sample_datas;

    while (!_audio_frame_datas.empty())
    {
        auto frame_data = _audio_frame_datas.front();

        if (start_timestamp == 0)
            start_timestamp = frame_data->timestamp;

        if ((!is_audio_timestamp && _audio_frame_datas.size() <= 1) || (frame_data->timestamp >= max_timestamp))
            break;

        _audio_frame_datas.pop_front();

        uint64_t duration = _audio_frame_datas.empty() ? (max_timestamp - frame_data->timestamp) :
                            (_audio_frame_datas.front()->timestamp - frame_data->timestamp);

        end_timestamp = _audio_frame_datas.empty() ? max_timestamp : _audio_frame_datas.front()->timestamp;

        frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

        auto sample_data = std::make_shared<FragmentSampleData>(duration, 0, 0, frame_data->data);

        sample_datas.push_back(sample_data);
    }

    // Fragment write
    auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::AudioMediaType,
                                                                4096,
                                                                _audio_sequence_number,
                                                                AUDIO_TRACK_ID,
                                                                start_timestamp,
                                                                sample_datas);

    fragment_writer->CreateData();

    std::ostringstream file_name_stream;
    file_name_stream << _segment_prefix.CStr() << "_" << start_timestamp << MPD_AUDIO_SUFFIX;

    ov::String file_name = file_name_stream.str().c_str();

    // m4s save
    SetSegmentData(file_name, end_timestamp - start_timestamp, start_timestamp, fragment_writer->GetDataStream());

    return true;
}

//====================================================================================================
// PlayList(mpd) Update
// - 차후 자동 인덱싱 사용시 참고 : LSN = floor(now - (availabilityStartTime + PST) / segmentDuration + startNumber - 1)
//====================================================================================================
bool DashPacketyzer::UpdatePlayList()
{
    std::ostringstream play_list_stream;
    std::ostringstream video_urls;
    std::ostringstream audio_urls;
    uint64_t video_total_duration = 0;
    uint64_t audio_total_duration = 0;

    std::vector<std::shared_ptr<SegmentData>> video_segment_datas;
    Packetyzer::GetVideoPlaySegments(video_segment_datas);

    for(uint32_t index = 0; index < video_segment_datas.size(); index++)
    {
        // Timeline Setting
        if(index == 0)
            video_urls  << "\t\t\t\t" << "<S t=\"" << video_segment_datas[index]->timestamp
                        << "\" d=\"" << video_segment_datas[index]->duration
                        << "\"/>\n";
        else
            video_urls << "\t\t\t\t" << "<S d=\"" << video_segment_datas[index]->duration << "\"/>\n";

        // total duration
        video_total_duration += video_segment_datas[index]->duration;
    }

    std::vector<std::shared_ptr<SegmentData>> audio_segment_datas;
    Packetyzer::GetAudioPlaySegments(audio_segment_datas);
    for(uint32_t index = 0; index < audio_segment_datas.size(); index++)
    {
        // Timeline Setting
        if(index == 0)
            audio_urls << "\t\t\t\t<S t=\"" << audio_segment_datas[index]->timestamp
                       << "\" d=\"" << audio_segment_datas[index]->duration
                       << "\"/>\n";
        else
            audio_urls << "\t\t\t\t<S d=\"" << audio_segment_datas[index]->duration << "\"/>\n";

        // total duration
        audio_total_duration += audio_segment_datas[index]->duration;
    }

    if (_start_time.empty())
    {
        _start_time = MakeUtcTimeString(time(nullptr));
    }

    double time_shift_buffer_depth = 0;

    if(video_urls.str().empty() && _media_info.audio_timescale != 0)
    {
        time_shift_buffer_depth = audio_total_duration / (double)_media_info.audio_timescale;
    }
    else if(_media_info.video_timescale != 0)
    {
        time_shift_buffer_depth = video_total_duration / (double)_media_info.video_timescale;
    }

    double current_milliseconds = GetCurrentMilliseconds();

    play_list_stream << std::fixed << std::setprecision(3)
            << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
            << "    xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
            << "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
            << "    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
            << "    type=\"dynamic\"\n"
            << "    minimumUpdatePeriod=\"PT"  << (current_milliseconds - _mpd_update_time)/1000 << "S\"\n"
            << "    publishTime=" << MakeUtcTimeString(time(nullptr)) << "\n"
            << "    availabilityStartTime=" << _start_time << "\n"
            << "    timeShiftBufferDepth=\"PT" << time_shift_buffer_depth << "S\"\n"
            << "    suggestedPresentationDelay=\"PT" << std::setprecision(1) << _mpd_suggested_presentation_delay << "S\"\n"
            << "    minBufferTime=\"PT" << _mpd_min_buffer_time << "S\">\n"
            << "<Period id=\"0\" start=\"PT0S\">\n";

    _mpd_update_time = current_milliseconds;

    // video listing
    if (!video_urls.str().empty())
    {
        play_list_stream << "\t<AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" width=\"" << _media_info.video_width
            << "\" height=\"" << _media_info.video_height
            << "\" par=\"" << _mpd_pixel_aspect_ratio << "\" frameRate=\"" << _media_info.video_framerate
            << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">"
            << "\n"
            << "\t\t<SegmentTemplate timescale=\"" << _media_info.video_timescale
            << "\" initialization=\"" << MPD_VIDEO_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << MPD_VIDEO_SUFFIX << "\">\n"
            << "\t\t\t<SegmentTimeline>\n"
            << video_urls.str()
            << "\t\t\t</SegmentTimeline>\n"
            << "\t\t</SegmentTemplate>\n"
            << "\t\t<Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _media_info.video_bitrate
            << "\" />\n"
            << "\t</AdaptationSet>\n";
    }

    // audio listing
    if (!audio_urls.str().empty())
    {
        play_list_stream << "\t<AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
            << "\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\""
            << _media_info.audio_channels << "\"/>\n"
            << "\t\t<SegmentTemplate timescale=\"" << _media_info.audio_timescale
            << "\" initialization=\"" << MPD_AUDIO_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << MPD_AUDIO_SUFFIX << "\">\n"
            << "\t\t\t<SegmentTimeline>\n"
            << audio_urls.str()
            << "\t\t\t</SegmentTimeline>\n"
            << "\t\t</SegmentTemplate>\n"
            << "\t\t<Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _media_info.audio_samplerate
            << "\" bandwidth=\"" << _media_info.audio_bitrate << "\" />\n"
            << "\t</AdaptationSet>\n";
    }

    play_list_stream << "</Period>\n" << "</MPD>\n";

    ov::String play_list = play_list_stream.str().c_str();
    SetPlayList(play_list);

    if(_stream_type == PacketyzerStreamType::Common && _init_segment_count_complete)
    {
        if(video_urls.str().empty())
            logtw("Dash video segment problems - %s/%s", _app_name.CStr(), _stream_name.CStr());

        if(audio_urls.str().empty())
            logtw("Dash audio segment problems - %s/%s", _app_name.CStr(), _stream_name.CStr());
    }

    return true;
}

//====================================================================================================
// Get Segment
//====================================================================================================
bool DashPacketyzer::GetSegmentData(const ov::String &file_name, std::shared_ptr<ov::Data> &data)
{
    if(!_init_segment_count_complete)
        return false;

    // video init file
    if(file_name == MPD_VIDEO_INIT_FILE_NAME)
    {
        if(_mpd_video_init_file == nullptr)
        {
            data = nullptr;
            return false;
        }

        data =_mpd_video_init_file->data;
        return true;
    }
    // audio init file
    else if(file_name == MPD_AUDIO_INIT_FILE_NAME)
    {
        if(_mpd_audio_init_file == nullptr)
        {
            data = nullptr;
            return false;
        }

        data =_mpd_audio_init_file->data;
        return true;
    }

    if (file_name.IndexOf(MPD_VIDEO_SUFFIX) >= 0)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        auto item = std::find_if(_video_segment_datas.begin(), _video_segment_datas.end(),
                [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

        if(item == _video_segment_datas.end())
        {
            data = nullptr;
            return false;
        }

        data = (*item)->data;
        return data != nullptr;
    }
    else if (file_name.IndexOf(MPD_AUDIO_SUFFIX) >= 0)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        auto item = std::find_if(_audio_segment_datas.begin(), _audio_segment_datas.end(),
                [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

        if(item == _audio_segment_datas.end())
        {
            data = nullptr;
            return false;
        }

        data = (*item)->data;
        return data != nullptr;
    }

    data = nullptr;
    return false;
}

//====================================================================================================
// Set Segment
//====================================================================================================
bool DashPacketyzer::SetSegmentData(ov::String file_name,
                                   uint64_t duration,
                                   uint64_t timestamp,
                                   const std::shared_ptr<std::vector<uint8_t>> &data)
{
    if(file_name == MPD_VIDEO_INIT_FILE_NAME)
    {
        _mpd_video_init_file = std::make_shared<SegmentData>(0, MPD_VIDEO_INIT_FILE_NAME, duration, timestamp, data);
        return true;
    }
    else if(file_name == MPD_AUDIO_INIT_FILE_NAME)
    {
        _mpd_audio_init_file = std::make_shared<SegmentData>(0, MPD_AUDIO_INIT_FILE_NAME, duration, timestamp, data);
        return true;
    }

    if (file_name.IndexOf(MPD_VIDEO_SUFFIX) >= 0)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        _video_segment_datas[_current_video_index++] =
                std::make_shared<SegmentData>(_sequence_number++, file_name, duration, timestamp, data);

        if(_segment_save_count <= _current_video_index)
            _current_video_index = 0;

        _video_sequence_number++;
    }
    else if (file_name.IndexOf(MPD_AUDIO_SUFFIX) >= 0)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        _audio_segment_datas[_current_audio_index++] =
                std::make_shared<SegmentData>(_sequence_number++, file_name, duration, timestamp, data);

        if(_segment_save_count <= _current_audio_index)
            _current_audio_index = 0;

        _audio_sequence_number++;
    }

    if(!_init_segment_count_complete &&
            (_video_sequence_number > _segment_count || _audio_sequence_number > _segment_count))
    {
        _init_segment_count_complete = true;
        logti("Dash ready completed - prefix(%s) segment(%ds/%d)",
                _segment_prefix.CStr(),
                _segment_duration,
                _segment_count);
    }

    return true;
}

