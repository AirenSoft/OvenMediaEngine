//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_packetyzer.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <numeric>
#include "cmaf_private.h"

#define VIDEO_TRACK_ID    (1)
#define AUDIO_TRACK_ID    (2)

#define MPD_AUDIO_SUFFIX            "_audio_cmaf.m4s"
#define MPD_VIDEO_SUFFIX            "_video_cmaf.m4s"
#define MPD_VIDEO_INIT_FILE_NAME    "init_video_cmaf.m4s"
#define MPD_AUDIO_INIT_FILE_NAME    "init_audio_cmaf.m4s"


//====================================================================================================
// Constructor
//====================================================================================================
CmafPacketyzer::CmafPacketyzer(const ov::String &app_name,
                            const ov::String &stream_name,
                            PacketyzerStreamType stream_type,
                            const ov::String &segment_prefix,
                            uint32_t segment_count,
                            uint32_t segment_duration,
                            PacketyzerMediaInfo &media_info) :
                                Packetyzer(app_name,
                                            stream_name,
                                            PacketyzerType::Cmaf,
                                            stream_type,
                                            segment_prefix,
                                            segment_count,
                                            segment_duration,
                                            media_info)
{
    _avc_nal_header_size = 0;


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

    _video_segment_writer = std::make_unique<CmafM4sFragmentWriter>(M4sMediaType::Video, 1, VIDEO_TRACK_ID);
    _audio_segment_writer = std::make_unique<CmafM4sFragmentWriter>(M4sMediaType::Audio, 1, AUDIO_TRACK_ID);

}

//====================================================================================================
// Destructor
//====================================================================================================
CmafPacketyzer::~CmafPacketyzer()
{

}

//====================================================================================================
// Video 설정 값 Load ( Key Frame 데이터만 가능)
//  0001 + sps + 0001 + pps + 0001 + I-Frame 구조 파싱
// - profile
// - level
// - sps/pps
// - init m4s 생성
//====================================================================================================
bool CmafPacketyzer::VideoInit(const std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    uint32_t current_index = 0;
    int sps_start_index = -1;
    int sps_end_index = -1;
    int pps_start_index = -1;
    int pps_end_index = -1;
    int total_start_pattern_size = 0;

    auto data = std::make_shared<std::vector<uint8_t>>(frame_data->data->GetDataAs<uint8_t>(),
                    frame_data->data->GetDataAs<uint8_t>() + frame_data->data->GetLength());

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
        logte("Cmaf packetyzer sps/pps pars fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
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
        logte("Cmaf video init writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    _avc_nal_header_size = avc_sps->size() + avc_pps->size() + total_start_pattern_size;

    // Video init m4s Save
    SetSegmentData(MPD_VIDEO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());


    // Set video time priority over audio
    _start_time = GetCurrentMilliseconds();
    _start_time_string = MakeUtcTimeString(time(nullptr));

    return true;
}

//====================================================================================================
// Audio 설정 값 Load
// - sample rate
// - channels
// - init m4s 생성
//====================================================================================================
bool CmafPacketyzer::AudioInit()
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
        logte("Cmaf audio init writer create fail - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());
        return false;
    }

    // Audio init m4s Save
    SetSegmentData(MPD_AUDIO_INIT_FILE_NAME, 0, 0, writer->GetDataStream());

    if (_start_time == 0)
    {
        _start_time = GetCurrentMilliseconds();
        _start_time_string = MakeUtcTimeString(time(nullptr));
    }

    return true;
}

//====================================================================================================
// Video Frame Append
// - Video Segment m4s Create
//====================================================================================================
bool CmafPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (!_video_init)
    {
        if((frame_data->type != PacketyzerFrameType::VideoKeyFrame) || !VideoInit(frame_data))
            return false;

        _video_init = true;
        logtd("Cmaf video packetyzer init - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        // nal header offset skip
        frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
                                                     _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

        _previous_video_frame = frame_data;
        return true;
    }

    auto sample_data = std::make_shared<FragmentSampleData>(frame_data->timestamp - _previous_video_frame->timestamp,
                                                            _previous_video_frame->type == PacketyzerFrameType::VideoKeyFrame
                                                            ? 0X02000000 : 0X01010000,
                                                            _previous_video_frame->timestamp,
                                                            _previous_video_frame->time_offset,
                                                            _previous_video_frame->data);

    _video_segment_writer->AppendSample(sample_data);

    // nal header offset skip
    frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
                                                 _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

    _previous_video_frame = frame_data;

    double current_time = GetCurrentMilliseconds();
    uint32_t current_number = (current_time - _start_time)/(_segment_duration*1000);

    // - KeyFrame ~ KeyFrame(before)
    if (frame_data->type != PacketyzerFrameType::VideoKeyFrame ||  current_number <_video_sequence_number)
    {
        return true;
    }

    logtd("Cmaf video segment write - stream(%s/%s) number(%d) gap(%.0fms)",
          _app_name.CStr(),
          _stream_name.CStr(),
          _video_sequence_number,
          current_time - (_start_time +  _segment_duration*_video_sequence_number*1000));

    if(current_number > _video_sequence_number)
    {
        logtw("Cmaf video segment sequence number change - stream(%s/%s) number(%d => %d) gap(%.0fms)",
              _app_name.CStr(),
              _stream_name.CStr(),
              _video_sequence_number,
              current_number,
              current_time - (_start_time +  _segment_duration*_video_sequence_number*1000));

        _video_sequence_number = current_number;
    }

    VideoSegmentWrite(frame_data->timestamp);

    UpdatePlayList();

    return true;
}

//====================================================================================================
// Audio Frame Append
// - Audio Segment m4s Create
//====================================================================================================
bool CmafPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    if (!_audio_init)
    {
        if(!AudioInit())
            return false;

        _audio_init = true;
        logtd("Cmaf audio packetyzer init - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        // adts offset skip
        frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

        _previous_audio_frame = frame_data;
        return true;
    }



    auto sample_data = std::make_shared<FragmentSampleData>(frame_data->timestamp - _previous_audio_frame->timestamp,
                                                            0,
                                                            _previous_audio_frame->timestamp,
                                                            0,
                                                            _previous_audio_frame->data);

    _audio_segment_writer->AppendSample(sample_data);

    // adts offset skip
    frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

    _previous_audio_frame = frame_data;

    double current_time = GetCurrentMilliseconds();
    uint32_t current_number = (current_time - _start_time)/(_segment_duration*1000);

    if (current_number <_audio_sequence_number)
    {
        return true;
    }

    logtd("Cmaf audio segment write - stream(%s/%s) number(%d) gap(%.0fms)",
          _app_name.CStr(),
          _stream_name.CStr(),
          _audio_sequence_number,
          current_time - (_start_time +  _segment_duration*_audio_sequence_number*1000));

    if(current_number > _audio_sequence_number)
    {
        logtw("Cmaf audio segment sequence number change - stream(%s/%s) number(%d => %d) gap(%.0fms)",
              _app_name.CStr(),
              _stream_name.CStr(),
              _audio_sequence_number,
              current_number,
              current_time - (_start_time +  _segment_duration*_audio_sequence_number*1000));

        _audio_sequence_number = current_number;
    }

    AudioSegmentWrite(frame_data->timestamp);

    UpdatePlayList();

    return true;
}

//====================================================================================================
// Video Segment Write
// - Duration/Key Frame 확인 이후 이전 데이터 까지 생성
//====================================================================================================
bool CmafPacketyzer::VideoSegmentWrite(uint64_t max_timestamp)
{
    ov::String file_name = ov::String::FormatString("%s_%u%s",
                                                    _segment_prefix.CStr(), _video_sequence_number, MPD_VIDEO_SUFFIX);

    uint64_t duration = max_timestamp - _video_segment_writer->GetStartTimestamp();

    // m4s data save
    SetSegmentData(file_name, duration, _video_segment_writer->GetStartTimestamp(), _video_segment_writer->GetSegment());

    _video_segment_writer->Clear();

    return true;
}

//====================================================================================================
// Audio Segment Write
// - 비디오 Segment 생성이후 생성 or Audio Only 에서 생성
//====================================================================================================
bool CmafPacketyzer::AudioSegmentWrite(uint64_t max_timestamp)
{
    ov::String file_name = ov::String::FormatString("%s_%u%s",
                                                    _segment_prefix.CStr(),
                                                    _audio_sequence_number,
                                                    MPD_AUDIO_SUFFIX);

    uint64_t duration = max_timestamp - _audio_segment_writer->GetStartTimestamp();

    // m4s data save
    SetSegmentData(file_name, duration, _audio_segment_writer->GetStartTimestamp(), _audio_segment_writer->GetSegment());

    _audio_segment_writer->Clear();

    return true;
}

//====================================================================================================
// PlayList(mpd) Update
// - 차후 자동 인덱싱 사용시 참고 : LSN = floor(now - (availabilityStartTime + PST) / segmentDuration + startNumber - 1)
//====================================================================================================
bool CmafPacketyzer::UpdatePlayList()
{
    std::ostringstream play_list_stream;
    ov::String video_urls;
    ov::String audio_urls;
    // double time_shift_buffer_depth = 0;
    double minimumUpdatePeriod = 30;

    play_list_stream
    << std::fixed << std::setprecision(3)
    << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
    << "    xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
    << "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
    << "    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
    << "    type=\"dynamic\"\n"
    << "    minimumUpdatePeriod=\"PT" << minimumUpdatePeriod << "S\"\n"
    << "    publishTime=" << MakeUtcTimeString(time(nullptr)) << "\n"
    << "    availabilityStartTime=" << _start_time_string << "\n"
    << "    timeShiftBufferDepth=\"PT15S\"\n"
    << "    suggestedPresentationDelay=\"PT" << _segment_duration << "S\"\n"
    << "    minBufferTime=\"PT" << _mpd_min_buffer_time << "S\">\n"
    << "<Period id=\"0\" start=\"PT0S\">\n";

    // video listing
    if (_video_sequence_number > 1)
    {
        play_list_stream
        << "    <AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" width=\"" << _media_info.video_width
        << "\" height=\"" << _media_info.video_height << "\" par=\"" << _mpd_pixel_aspect_ratio
        << "\" frameRate=\"" << _media_info.video_framerate
        << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"

        << "        <SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << _media_info.video_timescale
        << "\" duration=\"" << _segment_duration*_media_info.video_timescale
        << "\" startNumber=\"1\" initialization=\"" << MPD_VIDEO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << MPD_VIDEO_SUFFIX << "\" />\n"

        << "        <Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _media_info.video_bitrate  << "\" />\n"

        << "    </AdaptationSet>\n";
    }

    // audio listing
    if (_audio_sequence_number > 1)
    {
        play_list_stream
        << "    <AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" "
        << "startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"

        << "        <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" "
        << "value=\"" << _media_info.audio_channels << "\"/>\n"

        << "        <SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << _media_info.audio_timescale
        << "\" duration=\"" << _segment_duration*_media_info.audio_timescale
        << "\" startNumber=\"1\" initialization=\"" << MPD_AUDIO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << MPD_AUDIO_SUFFIX << "\" />\n"

        << "        <Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _media_info.audio_samplerate
        << "\" bandwidth=\"" << _media_info.audio_bitrate << "\" />\n"

        << "    </AdaptationSet>\n";
    }

    play_list_stream << "</Period>\n" << "</MPD>\n";

    ov::String play_list = play_list_stream.str().c_str();
    SetPlayList(play_list);
    return true;
}

//====================================================================================================
// Get Segment
//====================================================================================================
bool CmafPacketyzer::GetSegmentData(const ov::String &file_name, std::shared_ptr<ov::Data> &data)
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
bool CmafPacketyzer::SetSegmentData(ov::String file_name,
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

        logtd("Cmaf video segment add - stream(%s/%s) file(%s) duration(%0.3f)",
              _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), (double)duration/_media_info.video_timescale);

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

        logtd("Cmaf audio segment add - stream(%s/%s) file(%s) duration(%0.3f)",
              _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), (double)duration/_media_info.audio_timescale);
    }

    if(!_init_segment_count_complete &&
        (_video_sequence_number > _segment_count || _audio_sequence_number > _segment_count))
    {
        _init_segment_count_complete = true;
        logti("Cmaf ready completed - stream(%s/%s) prefix(%s) segment(%ds/%d)",
              _app_name.CStr(), _stream_name.CStr(), _segment_prefix.CStr(), _segment_duration, _segment_count);
    }

    return true;
}

