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

#define MPD_VIDEO_SUFFIX            "_video.m4s"
#define MPD_AUDIO_SUFFIX            "_audio.m4s"
#define MPD_VIDEO_INIT_FILE_NAME    "init_video.m4s"
#define MPD_AUDIO_INIT_FILE_NAME    "init_audio.m4s"

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

    _mpd_suggested_presentation_delay = _segment_duration;//(_segment_count / 2 * _segment_duration);
    _mpd_min_buffer_time = 6; //_segment_duration; // (double)_segment_duration/2*1.2;

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
// - init m4s create
//====================================================================================================
bool DashPacketyzer::VideoInit(std::shared_ptr<ov::Data> &frame_data)
{
    uint32_t current_index = 0;
    int sps_start_index = -1;
    int sps_end_index = -1;
    int pps_start_index = -1;
    int pps_end_index = -1;
    int total_start_pattern_size = 0;

    auto data = std::make_shared<std::vector<uint8_t>>(frame_data->GetDataAs<uint8_t>(),
            frame_data->GetDataAs<uint8_t>() + frame_data->GetLength());

    // sps/pps parsing
    while ((current_index + AVC_NAL_START_PATTERN_SIZE) < data->size())
    {
        int start_pattern_size = 0;

        // 0x00 0x00 0x00 0x01 pattern check
        if (data->at(current_index) == 0 &&
            data->at(current_index + 1) == 0 &&
            data->at(current_index + 2) == 0 &&
            data->at(current_index + 3) == 1)
        {
            start_pattern_size = 4;
            total_start_pattern_size += start_pattern_size;
        }
        // 0x00 0x00 0x00 0x01 pattern check
        else if(data->at(current_index) == 0 &&
                data->at(current_index + 1) == 0 &&
                data->at(current_index + 2) == 1)
        {
            start_pattern_size = 3;
            total_start_pattern_size += start_pattern_size;
        }
        else
        {
            current_index++;
            continue;
        }

        if (sps_start_index == -1)
        {
            sps_start_index = current_index + start_pattern_size;
            current_index += start_pattern_size;
            continue;
        }
        else if (sps_end_index == -1)
        {
            sps_end_index = current_index - 1;
            pps_start_index = current_index + start_pattern_size;
            current_index += start_pattern_size;
            continue;
        }
        else
        {
            pps_end_index = current_index - 1;
            break;
        }
    }

    // parsing result check
    if (!(sps_start_index >= 0 && sps_end_index >= (sps_start_index + 4))||
        !(pps_start_index > sps_end_index && pps_end_index > pps_start_index) ||
        !(total_start_pattern_size >= 9 && total_start_pattern_size <= 12))
    {
        logte("Dash packetyzer sps/pps pars fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    // SPS save
    auto avc_sps = std::make_shared<std::vector<uint8_t>>(data->begin() + sps_start_index,
                                                          data->begin() + sps_end_index + 1);

    // PPS save
    auto avc_pps = std::make_shared<std::vector<uint8_t>>(data->begin() + pps_start_index,
                                                          data->begin() + pps_end_index + 1);

    // Video init m4s Create
    auto writer = std::make_unique<M4sInitWriter>(M4sMediaType::Video,
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
        logte("Dash video init writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    _avc_nal_header_size = avc_sps->size() + avc_pps->size() + total_start_pattern_size;

    // Video init m4s Save
    SetSegmentData(MPD_VIDEO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());

    return true;
}

//====================================================================================================
// Audio 설정 값 Load
// - sample rate
// - channels
// - init m4s create
//====================================================================================================
bool DashPacketyzer::AudioInit()
{
    std::shared_ptr<std::vector<uint8_t>> temp = nullptr;

    // Audio init m4s 생성(메모리)
    auto writer = std::make_unique<M4sInitWriter>(M4sMediaType::Audio,
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
        logte("Dash audio init writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    // Audio init m4s Save
    SetSegmentData(MPD_AUDIO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());

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
        if (frame_data->type != PacketyzerFrameType::VideoKeyFrame)
            return false;

        if(!VideoInit(frame_data->data))
            return false;

        _video_init = true;
    }

    // Fragment Check
    // - KeyFrame ~ KeyFrame(before)
    if (frame_data->type == PacketyzerFrameType::VideoKeyFrame && !_video_frame_datas.empty())
    {
        if (frame_data->timestamp - _video_frame_datas[0]->timestamp >=
                (_segment_duration * _media_info.video_timescale))
        {

            VideoSegmentWrite(frame_data->timestamp);

            if (!_audio_frame_datas.empty())
            {
                AudioSegmentWrite(ConvertTimeScale(frame_data->timestamp,
                                                    _media_info.video_timescale,
                                                    _media_info.audio_timescale));
            }

            UpdatePlayList();
        }
    }

    if (_start_time.empty())
    {
        _start_time = MakeUtcTimeString(time(nullptr));
    }

    // nal header offset skip
    frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
                                           _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

    _video_frame_datas.push_back(frame_data);
    _last_video_append_time = time(nullptr);

    if (frame_data->type == PacketyzerFrameType::VideoKeyFrame)
    {
        logtd("dash video timestamp %lld", frame_data->timestamp);
    }

    return true;
}

//====================================================================================================
// Audio Frame Append
// - Audio Segment m4s Create
//====================================================================================================
bool DashPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (!_audio_init)
    {
        if(!AudioInit())
            return false;

        _audio_init = true;
    }

    if (_start_time.empty())
    {
        _start_time = MakeUtcTimeString(time(nullptr));
    }

    // adts offset skip
    frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

    _audio_frame_datas.push_back(frame_data);
    _last_audio_append_time = time(nullptr);

    // audio only or video input error
    if ((time(nullptr) - _last_video_append_time >=
            static_cast<uint32_t>(_segment_duration)) && !_audio_frame_datas.empty())
    {
        if ((frame_data->timestamp - _audio_frame_datas[0]->timestamp) >=
                (_segment_duration * _media_info.audio_timescale))
        {
            AudioSegmentWrite(frame_data->timestamp);

            UpdatePlayList();
        }
    }

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

        auto sample_data = std::make_shared<FragmentSampleData>(duration,
                                                                frame_data->type == PacketyzerFrameType::VideoKeyFrame
                                                                ? 0X02000000 : 0X01010000,
                                                                frame_data->timestamp,
                                                                frame_data->time_offset,
                                                                frame_data->data);

        sample_datas.push_back(sample_data);
    }
    _video_frame_datas.clear();

    // Fragment write
    auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::Video,
                                                                _video_sequence_number,
                                                                VIDEO_TRACK_ID,
                                                                start_timestamp);

    if(fragment_writer->AppendSamples(sample_datas) <= 0)
    {
        logte("Dash video writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    // m4s data save
    SetSegmentData(ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, MPD_VIDEO_SUFFIX),
                max_timestamp - start_timestamp,
                start_timestamp,
                fragment_writer->GetDataStream());

    return true;
}

//====================================================================================================
// Audio Segment Write
// - 비디오 Segment 생성이후 생성 or Audio Only 에서 생성
//====================================================================================================
#define AAC_SAMPLES_PER_FRAME (1024)
bool DashPacketyzer::AudioSegmentWrite(uint64_t max_timestamp)
{
    uint64_t start_timestamp = 0;
    uint64_t end_timestamp = 0;
    std::vector<std::shared_ptr<FragmentSampleData>> sample_datas;

    //  size > 1  for duration calculation
    while (_audio_frame_datas.size() > 1)
    {
        auto frame_data = _audio_frame_datas.front();

        if (start_timestamp == 0)
            start_timestamp = frame_data->timestamp;

        if (((frame_data->timestamp + AAC_SAMPLES_PER_FRAME) > max_timestamp))
            break;

        _audio_frame_datas.pop_front();

        auto sample_data = std::make_shared<FragmentSampleData>(
                _audio_frame_datas.front()->timestamp - frame_data->timestamp,
                0,
                frame_data->timestamp,
                0,
                frame_data->data);

        end_timestamp = _audio_frame_datas.front()->timestamp;

        sample_datas.push_back(sample_data);
    }

    // Fragment write
    auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::Audio,
                                                                _audio_sequence_number,
                                                                AUDIO_TRACK_ID,
                                                                start_timestamp);

    if(fragment_writer->AppendSamples(sample_datas) <= 0)
    {
        logte("Dash audio writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    // m4s save
    SetSegmentData(ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, MPD_AUDIO_SUFFIX),
                    end_timestamp - start_timestamp,
                    start_timestamp,
                    fragment_writer->GetDataStream());

    return true;
}

//====================================================================================================
// GetSegmentPlayInfos
// - video/audio mpd timeline
//====================================================================================================
bool DashPacketyzer::GetSegmentPlayInfos(ov::String &video_urls,
                                        ov::String &audio_urls,
                                        double &time_shift_buffer_depth,
                                        double &minimumUpdatePeriod)
{
    std::ostringstream video_urls_stream;
    std::ostringstream audio_urls_stream;
    uint64_t video_total_duration = 0;
    uint64_t video_last_duration = 0;
    uint64_t audio_total_duration = 0;
    uint64_t audio_last_duration = 0;
    std::vector<std::shared_ptr<SegmentData>> video_segment_datas;
    std::vector<std::shared_ptr<SegmentData>> audio_segment_datas;

    Packetyzer::GetVideoPlaySegments(video_segment_datas);
    Packetyzer::GetAudioPlaySegments(audio_segment_datas);

    for(uint32_t index = 0; index < video_segment_datas.size(); index++)
    {
        // Timeline Setting
        if(index == 0)
            video_urls_stream  << "\t\t\t\t" << "<S t=\"" << video_segment_datas[index]->timestamp
                        << "\" d=\"" << video_segment_datas[index]->duration
                        << "\"/>\n";
        else
            video_urls_stream << "\t\t\t\t" << "<S d=\"" << video_segment_datas[index]->duration << "\"/>\n";

        // total duration
        video_total_duration += video_segment_datas[index]->duration;

        video_last_duration = video_segment_datas[index]->duration;
    }

    video_urls = video_urls_stream.str().c_str();

    for(uint32_t index = 0; index < audio_segment_datas.size(); index++)
    {
        // Timeline Setting
        if(index == 0)
            audio_urls_stream << "\t\t\t\t<S t=\"" << audio_segment_datas[index]->timestamp
                       << "\" d=\"" << audio_segment_datas[index]->duration
                       << "\"/>\n";
        else
            audio_urls_stream << "\t\t\t\t<S d=\"" << audio_segment_datas[index]->duration << "\"/>\n";

        // total duration
        audio_total_duration += audio_segment_datas[index]->duration;

        audio_last_duration = audio_segment_datas[index]->duration;
    }

    audio_urls = audio_urls_stream.str().c_str();

    if(!video_urls.IsEmpty() && _media_info.video_timescale != 0)
    {
        time_shift_buffer_depth = video_total_duration / (double)_media_info.video_timescale;
        minimumUpdatePeriod = video_last_duration / (double)_media_info.video_timescale;
    }
    else if(!audio_urls.IsEmpty() && _media_info.audio_timescale != 0)
    {
        time_shift_buffer_depth = audio_total_duration / (double)_media_info.audio_timescale;
        minimumUpdatePeriod = audio_last_duration / (double)_media_info.audio_timescale;
    }

    return true;
}

//====================================================================================================
// PlayList(mpd) Update
// - 차후 자동 인덱싱 사용시 참고 : LSN = floor(now - (availabilityStartTime + PST) / segmentDuration + startNumber - 1)
//====================================================================================================
bool DashPacketyzer::UpdatePlayList()
{
    std::ostringstream play_list_stream;
    ov::String video_urls;
    ov::String audio_urls;
    double time_shift_buffer_depth = 0;
    double minimumUpdatePeriod = 0;

    GetSegmentPlayInfos(video_urls, audio_urls, time_shift_buffer_depth, minimumUpdatePeriod);

    play_list_stream << std::fixed << std::setprecision(3)
            << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
            << "    xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
            << "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
            << "    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
            << "    type=\"dynamic\"\n"
            << "    minimumUpdatePeriod=\"PT" << minimumUpdatePeriod << "S\"\n"
            << "    publishTime=" << MakeUtcTimeString(time(nullptr)) << "\n"
            << "    availabilityStartTime=" << _start_time << "\n"
            << "    timeShiftBufferDepth=\"PT" << time_shift_buffer_depth << "S\"\n"
            << "    suggestedPresentationDelay=\"PT" << std::setprecision(1) << _mpd_suggested_presentation_delay << "S\"\n"
            << "    minBufferTime=\"PT" << _mpd_min_buffer_time << "S\">\n"
            << "<Period id=\"0\" start=\"PT0S\">\n";

    // video listing
    if (!video_urls.IsEmpty())
    {
        play_list_stream << "\t<AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" width=\"" << _media_info.video_width
            << "\" height=\"" << _media_info.video_height
            << "\" par=\"" << _mpd_pixel_aspect_ratio << "\" frameRate=\"" << _media_info.video_framerate
            << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">"
            << "\n"
            << "\t\t<SegmentTemplate timescale=\"" << _media_info.video_timescale
            << "\" initialization=\"" << MPD_VIDEO_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << MPD_VIDEO_SUFFIX << "\">\n"
            << "\t\t\t<SegmentTimeline>\n"
            << video_urls.CStr()
            << "\t\t\t</SegmentTimeline>\n"
            << "\t\t</SegmentTemplate>\n"
            << "\t\t<Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _media_info.video_bitrate
            << "\" />\n"
            << "\t</AdaptationSet>\n";
    }

    // audio listing
    if (!audio_urls.IsEmpty())
    {
        play_list_stream << "\t<AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
            << "\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\""
            << _media_info.audio_channels << "\"/>\n"
            << "\t\t<SegmentTemplate timescale=\"" << _media_info.audio_timescale
            << "\" initialization=\"" << MPD_AUDIO_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << MPD_AUDIO_SUFFIX << "\">\n"
            << "\t\t\t<SegmentTimeline>\n"
            << audio_urls.CStr()
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
        if(video_urls.IsEmpty())
            logtw("Dash video segment urls empty - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        if(audio_urls.IsEmpty())
            logtw("Dash audio segment urls empty - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
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

        _video_segment_datas[_current_video_index++] = std::make_shared<SegmentData>(_sequence_number++,
                                                                                    file_name,
                                                                                    duration,
                                                                                    timestamp,
                                                                                    data);

        if(_segment_save_count <= _current_video_index)
            _current_video_index = 0;

        _video_sequence_number++;

        logtd("Dash video segment add - stream(%s/%s) file(%s) duration(%lld/%0.3f)",
          _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, (double)duration/_media_info.video_timescale);

    }
    else if (file_name.IndexOf(MPD_AUDIO_SUFFIX) >= 0)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        _audio_segment_datas[_current_audio_index++] = std::make_shared<SegmentData>(_sequence_number++,
                                                                                    file_name,
                                                                                    duration,
                                                                                    timestamp,
                                                                                    data);

        if(_segment_save_count <= _current_audio_index)
            _current_audio_index = 0;

        _audio_sequence_number++;

        logtd("Dash audio segment add - stream(%s/%s) file(%s) duration(%lld/%0.3f)",
              _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, (double)duration/_media_info.audio_timescale);
    }

    if(!_init_segment_count_complete &&
            (_video_sequence_number > _segment_count || _audio_sequence_number > _segment_count))
    {
        _init_segment_count_complete = true;
        logti("Dash ready completed - stream(%s/%s) prefix(%s) segment(%ds/%d)",
              _app_name.CStr(), _stream_name.CStr(), _segment_prefix.CStr(), _segment_duration, _segment_count);
    }

    return true;
}

