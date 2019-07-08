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

//====================================================================================================
// Constructor
//====================================================================================================
CmafPacketyzer::CmafPacketyzer(const ov::String &app_name,
								const ov::String &stream_name,
								PacketyzerStreamType stream_type,
								const ov::String &segment_prefix,
								uint32_t segment_count,
								uint32_t segment_duration,
								PacketyzerMediaInfo &media_info,
							   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer) :
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

    _mpd_suggested_presentation_delay = 1;
    _mpd_min_buffer_time = _segment_duration - (_segment_duration*0.05);

	_mpd_video_availability_time_offset = _segment_duration;
	if(media_info.video_framerate != 0)
		_mpd_video_availability_time_offset = _segment_duration - (1/media_info.video_framerate);

	_mpd_audio_availability_time_offset = _segment_duration;
	if((media_info.audio_samplerate/1024) != 0)
		_mpd_audio_availability_time_offset = _segment_duration - (1/((double)media_info.audio_samplerate/1024));

    _video_segment_writer = std::make_unique<CmafChunkWriter>(M4sMediaType::Video, 1, VIDEO_TRACK_ID, true);
    _audio_segment_writer = std::make_unique<CmafChunkWriter>(M4sMediaType::Audio, 1, AUDIO_TRACK_ID, true);

	_chunked_transfer = chunked_transfer;

	_duration_margen = _segment_duration * 0.1*1000; // 10% -> ms

}

//====================================================================================================
// Destructor
//====================================================================================================
CmafPacketyzer::~CmafPacketyzer()
{

}

//====================================================================================================
// Current video file name
//====================================================================================================
ov::String CmafPacketyzer::GetCurrentVideoFileName()
{
	return ov::String::FormatString("%s_%u%s", _segment_prefix.CStr(), _video_sequence_number, CMAF_MPD_VIDEO_SUFFIX);
}

//====================================================================================================
// Current audio file name
//====================================================================================================
ov::String CmafPacketyzer::GetCurrentAudioFileName()
{
	return ov::String::FormatString("%s_%u%s", _segment_prefix.CStr(), _audio_sequence_number, CMAF_MPD_AUDIO_SUFFIX);
}


//====================================================================================================
// Video 설정 값 Load ( Key Frame 데이터만 가능)
//  0001 + sps + 0001 + pps + 0001 + I-Frame 구조 파싱
// - profile
// - level
// - sps/pps
// - init m4s 생성
//====================================================================================================
bool CmafPacketyzer::VideoInit(const std::shared_ptr<ov::Data> &frame_data)
{
    uint32_t current_index = 0;
    int sps_start_index = -1;
    int sps_end_index = -1;
    int pps_start_index = -1;
    int pps_end_index = -1;
    int total_start_pattern_size = 0;

    auto buffer = frame_data->GetDataAs<uint8_t>();

    // sps/pps parsing
    while ((current_index + AVC_NAL_START_PATTERN_SIZE) < frame_data->GetLength())
    {
        int start_pattern_size = 0;

        // 0x00 0x00 0x00 0x01 pattern check
		if (buffer[current_index] == 0 &&
			buffer[current_index + 1] == 0 &&
			buffer[current_index + 2] == 0 &&
			buffer[current_index + 3] == 1)
        {
            start_pattern_size = 4;
            total_start_pattern_size += start_pattern_size;
        }
        // 0x00 0x00 0x00 0x01 pattern check
		else if(buffer[current_index] == 0 &&
				buffer[current_index + 1] == 0 &&
				buffer[current_index + 2] == 1)
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
    auto avc_sps = frame_data->Subdata(sps_start_index, sps_end_index - sps_start_index + 1);

    // PPS save
    auto avc_pps = frame_data->Subdata(pps_start_index, pps_end_index - pps_start_index + 1);

	_avc_nal_header_size = avc_sps->GetLength() + avc_pps->GetLength() + total_start_pattern_size;

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



	auto init_data = writer->CreateData(true);

    // Video init m4s Save
    _mpd_video_init_file = std::make_shared<SegmentData>(0,CMAF_MPD_VIDEO_INIT_FILE_NAME, 0, 0, init_data);

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
    std::shared_ptr<ov::Data> temp = nullptr;

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

	auto init_data = writer->CreateData(true);

    // Audio init m4s Save
	_mpd_audio_init_file = std::make_shared<SegmentData>(0,CMAF_MPD_AUDIO_INIT_FILE_NAME, 0, 0, init_data);

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
        if((frame_data->type != PacketyzerFrameType::VideoKeyFrame) || !VideoInit(frame_data->data))
            return false;

        _video_init = true;
        logtd("Cmaf video packetyzer init - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        // nal header offset skip
        frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
                                                     _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

        _previous_video_frame = frame_data;
        return true;
    }

    auto sample_data = std::make_shared<SampleData>(frame_data->timestamp - _previous_video_frame->timestamp,
                                                            _previous_video_frame->type == PacketyzerFrameType::VideoKeyFrame
                                                            ? 0X02000000 : 0X01010000,
                                                            _previous_video_frame->timestamp,
                                                            _previous_video_frame->time_offset,
                                                            _previous_video_frame->data);

    auto chunk_data = _video_segment_writer->AppendSample(sample_data);

    // chunk data push
    // - (chunk frame) -> cmaf stream server -> response -> client
    if(chunk_data != nullptr && _chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetCurrentVideoFileName(), 0, chunk_data);
	}

    // nal header offset skip
    frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
                                                 _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

    _previous_video_frame = frame_data;

    double current_time = GetCurrentMilliseconds();
    uint32_t current_number = (current_time - _start_time + _duration_margen)/(_segment_duration*1000);

    // - KeyFrame ~ KeyFrame(before)
    if (frame_data->type != PacketyzerFrameType::VideoKeyFrame ||  current_number <_video_sequence_number)
    {
        return true;
    }

    VideoSegmentWrite(current_number, frame_data->timestamp);

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

    auto sample_data = std::make_shared<SampleData>(frame_data->timestamp - _previous_audio_frame->timestamp,
													0,
													_previous_audio_frame->timestamp,
													0,
													_previous_audio_frame->data);

    auto chunk_data = _audio_segment_writer->AppendSample(sample_data);

	// chunk data push notify
	if(chunk_data != nullptr && _chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetCurrentAudioFileName(), 0, chunk_data);
	}

    // adts offset skip
    frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

    _previous_audio_frame = frame_data;

    double current_time = GetCurrentMilliseconds();
    uint32_t current_number = (current_time - _start_time + _duration_margen)/(_segment_duration*1000);

    if (current_number <_audio_sequence_number)
    {
        return true;
    }

    AudioSegmentWrite(current_number, frame_data->timestamp);

    UpdatePlayList();

    return true;
}

//====================================================================================================
// Video Segment Write
// - Duration/Key Frame 확인 이후 이전 데이터 까지 생성
//====================================================================================================
bool CmafPacketyzer::VideoSegmentWrite(uint32_t current_number, uint64_t max_timestamp)
{
	if(current_number > _video_sequence_number)
	{
		logtw("Cmaf video segment sequence number change - stream(%s/%s) number(%d => %d)",
			  _app_name.CStr(), _stream_name.CStr(), _video_sequence_number, current_number);

		// creating chunk data created notify
		// - sequence change exception
		if(_chunked_transfer != nullptr)
			_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, GetCurrentVideoFileName());

		_video_sequence_number = current_number;
	}

    auto file_name = GetCurrentVideoFileName();

	auto segment = _video_segment_writer->GetChunkedSegment();

	// m4s data save
    SetSegmentData(file_name,
					max_timestamp - _video_segment_writer->GetStartTimestamp(),
					_video_segment_writer->GetStartTimestamp(),
				   segment);

	_video_segment_writer->Clear();

	// creating chunk data created notify
	if(_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name);
	}

    return true;
}

//====================================================================================================
// Audio Segment Write
// - 비디오 Segment 생성이후 생성 or Audio Only 에서 생성
//====================================================================================================
bool CmafPacketyzer::AudioSegmentWrite(uint32_t current_number, uint64_t max_timestamp)
{
	if(current_number > _audio_sequence_number)
	{
		logtw("Cmaf audio segment sequence number change - stream(%s/%s) number(%d => %d)",
			  _app_name.CStr(), _stream_name.CStr(), _audio_sequence_number, current_number);

		// creating chunk data created notify
		// - sequence change exception
		if(_chunked_transfer != nullptr)
			_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, GetCurrentAudioFileName());

		_audio_sequence_number = current_number;
	}

	auto file_name = GetCurrentAudioFileName();

	auto segment = _audio_segment_writer->GetChunkedSegment();

	// m4s data save
    SetSegmentData(file_name,
					max_timestamp - _audio_segment_writer->GetStartTimestamp(),
					_audio_segment_writer->GetStartTimestamp(),
				   segment);

    _audio_segment_writer->Clear();

	// creating chunk data created notify
	if(_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name);
	}

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
    // double minimumUpdatePeriod = 30;

    play_list_stream
    << std::fixed << std::setprecision(3)
    << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
    << "    xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
    << "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
    << "    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
    << "    type=\"dynamic\"\n"
    << "    minimumUpdatePeriod=\"P100Y\"\n" //<< minimumUpdatePeriod << "S\"\n"
    << "    publishTime=" << MakeUtcTimeString(time(nullptr)) << "\n"
    << "    availabilityStartTime=" << _start_time_string << "\n"
    << "    timeShiftBufferDepth=\"PT6S\"\n"
    << "    suggestedPresentationDelay=\"PT" << _mpd_suggested_presentation_delay << "S\"\n"
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
        << "\" duration=\"" << (uint32_t)_segment_duration*_media_info.video_timescale
        << "\" availabilityTimeOffset=\"" << _mpd_video_availability_time_offset
        << "\" startNumber=\"1\" initialization=\"" << CMAF_MPD_VIDEO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_VIDEO_SUFFIX << "\" />\n"

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
        << "\" duration=\"" << (uint32_t)_segment_duration*_media_info.audio_timescale
		<< "\" availabilityTimeOffset=\"" << _mpd_audio_availability_time_offset
        << "\" startNumber=\"1\" initialization=\"" << CMAF_MPD_AUDIO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_AUDIO_SUFFIX << "\" />\n"

        << "        <Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _media_info.audio_samplerate
        << "\" bandwidth=\"" << _media_info.audio_bitrate << "\" />\n"

        << "    </AdaptationSet>\n";
    }

    play_list_stream << "</Period>\n" << "</MPD>\n";

    ov::String play_list = play_list_stream.str().c_str();
    SetPlayList(play_list);


	// segment start availability
	if(_init_segment_count_complete == false && _video_init == true && _audio_init == true)
	{
		_init_segment_count_complete = true;
		logti("Cmaf segment ready completed(ull) - stream(%s/%s)  segment(%ds/%d)",
			  _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

    return true;
}

//====================================================================================================
// Get Segment
//====================================================================================================
const std::shared_ptr<SegmentData> CmafPacketyzer::GetSegmentData(const ov::String &file_name)
{
    if(!_init_segment_count_complete)
        return nullptr;

    // video init file
    if(file_name == CMAF_MPD_VIDEO_INIT_FILE_NAME)
    {
        return _mpd_video_init_file;
    }
    // audio init file
    else if(file_name == CMAF_MPD_AUDIO_INIT_FILE_NAME)
    {
       return _mpd_audio_init_file;
    }

    if (file_name.IndexOf(CMAF_MPD_VIDEO_SUFFIX) >= 0)
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
           return nullptr;
        }

        return (*item);
    }
    else if (file_name.IndexOf(CMAF_MPD_AUDIO_SUFFIX) >= 0)
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
           return nullptr;
        }

        return (*item);
    }

    return nullptr;
}

//====================================================================================================
// Set Segment
// - http chunked transfer info input( size \r\n data \r\n .........0\r\n\r\n
//====================================================================================================
bool CmafPacketyzer::SetSegmentData(ov::String file_name,
									uint64_t duration,
									uint64_t timestamp,
									std::shared_ptr<ov::Data> &data)
{
 	if (file_name.IndexOf(CMAF_MPD_VIDEO_SUFFIX) >= 0)
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
    else if (file_name.IndexOf(CMAF_MPD_AUDIO_SUFFIX) >= 0)
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

    return true;
}

