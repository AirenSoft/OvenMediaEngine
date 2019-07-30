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

	_video_chunk_writer = std::make_unique<CmafChunkWriter>(M4sMediaType::Video, 1, VIDEO_TRACK_ID, true);
	_audio_chunk_writer = std::make_unique<CmafChunkWriter>(M4sMediaType::Audio, 1, AUDIO_TRACK_ID, true);

	_chunked_transfer = chunked_transfer;
}

//====================================================================================================
// Destructor
//====================================================================================================
CmafPacketyzer::~CmafPacketyzer()
{

}

//====================================================================================================
// Cmaf File Type
//====================================================================================================
CmafFileType CmafPacketyzer::GetFileType(const ov::String &file_name)
{
	if (file_name == CMAF_MPD_VIDEO_INIT_FILE_NAME)
		return CmafFileType::VideoInit;
	else if (file_name == CMAF_MPD_AUDIO_INIT_FILE_NAME)
		return CmafFileType::AudioInit;
	else if (file_name.IndexOf(CMAF_MPD_VIDEO_SUFFIX) >= 0)
		return CmafFileType::VideoSegment;
	else if (file_name.IndexOf(CMAF_MPD_AUDIO_SUFFIX) >= 0)
		return CmafFileType::AudioSegment;

	return CmafFileType::Unknown;
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
	_video_init_file = std::make_shared<SegmentData>(0,CMAF_MPD_VIDEO_INIT_FILE_NAME, 0, 0, init_data);

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
	_audio_init_file = std::make_shared<SegmentData>(0,CMAF_MPD_AUDIO_INIT_FILE_NAME, 0, 0, init_data);

    return true;
}

//====================================================================================================
// Video Frame Append
// - Video Segment m4s Create
//====================================================================================================
bool CmafPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
	// nal header offset skip
	frame_data->data = frame_data->data->Subdata((frame_data->type == PacketyzerFrameType::VideoKeyFrame) ?
												 _avc_nal_header_size : AVC_NAL_START_PATTERN_SIZE);

	if (!_video_init)
    {
        if((frame_data->type != PacketyzerFrameType::VideoKeyFrame) || !VideoInit(frame_data->data))
            return false;

        _video_init = true;
        logtd("Cmaf video packetyzer init - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        _previous_video_frame = frame_data;

        return true;
    }

	// available start time setting(first key frame)
	if(!_is_video_time_base)
	{
		_is_video_time_base = true;

		_available_start_time = GetCurrentMilliseconds();

		_video_start_timestamp = _previous_video_frame->timestamp;
		_video_last_timestamp = _previous_video_frame->timestamp;

		// audio chunk clear
		if(_audio_chunk_writer != nullptr)
		{
			_audio_chunk_writer->Clear();

			_audio_start_timestamp =
					ConvertTimeScale(_video_start_timestamp, _media_info.video_timescale, _media_info.audio_timescale);

			_audio_last_timestamp = _audio_start_timestamp;
		}
	}

	if (_previous_video_frame->type ==  PacketyzerFrameType::VideoKeyFrame )
	{
		// segment start frame is key frame
		uint32_t current_number =
				(_previous_video_frame->timestamp - _video_start_timestamp)/
				(_segment_duration*_media_info.video_timescale);

		if(current_number >= _video_sequence_number)
		{
			VideoSegmentWrite(current_number, _previous_video_frame->timestamp);
		}
	}

    auto sample_data = std::make_shared<SampleData>(frame_data->timestamp - _previous_video_frame->timestamp,
							_previous_video_frame->type == PacketyzerFrameType::VideoKeyFrame ? 0X02000000 : 0X01010000,
							_previous_video_frame->timestamp,
							_previous_video_frame->time_offset,
							_previous_video_frame->data);

    auto chunk_data = _video_chunk_writer->AppendSample(sample_data);

    // chunk data push
    // - (chunk frame) -> cmaf stream server -> response -> client
    if(chunk_data != nullptr && _chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetCurrentVideoFileName(), true, chunk_data);
	}

	_video_last_timestamp = sample_data->timestamp;

	_video_last_tick = GetCurrentTick();

    _previous_video_frame = frame_data;

    return true;
}

//====================================================================================================
// Audio Frame Append
// - Audio Segment m4s Create
//====================================================================================================
bool CmafPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
	// adts offset skip
	frame_data->data = frame_data->data->Subdata(ADTS_HEADER_SIZE);

	if (!_audio_init)
    {
        if(!AudioInit())
            return false;

        _audio_init = true;
        logtd("Cmaf audio packetyzer init - stream(%s/%s)", _app_name.CStr(), _stream_name.CStr());

        _previous_audio_frame = frame_data;

        return true;
    }

	// available start time setting
	if (_available_start_time == 0)
	{
		_available_start_time =  GetCurrentMilliseconds();
		_audio_start_timestamp = _previous_audio_frame->timestamp;
		_audio_last_timestamp = _previous_audio_frame->timestamp;
	}

	// segment update check
	uint32_t current_number =
			(_previous_audio_frame->timestamp - _audio_start_timestamp)/(_segment_duration*_media_info.audio_timescale);

	if(current_number >= _audio_sequence_number)
	{
		AudioSegmentWrite(current_number, _previous_audio_frame->timestamp);
	}

	auto sample_data = std::make_shared<SampleData>(frame_data->timestamp - _previous_audio_frame->timestamp,
													_previous_audio_frame->timestamp,
													_previous_audio_frame->data);

    auto chunk_data = _audio_chunk_writer->AppendSample(sample_data);

	// chunk data push notify
	if(chunk_data != nullptr && _chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetCurrentAudioFileName(), false, chunk_data);
	}

	_audio_last_timestamp = sample_data->timestamp;

	_audio_last_tick = GetCurrentTick();

    _previous_audio_frame = frame_data;

    return true;
}

//====================================================================================================
// Video Segment Write
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
		{
			_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, GetCurrentVideoFileName(), true);
		}

		_video_sequence_number = current_number;
	}

	uint64_t duration = max_timestamp - _video_chunk_writer->GetStartTimestamp();
	auto file_name = GetCurrentVideoFileName();

	// creating chunk data created notify
	if(_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, true);
	}


	// gap 100ms over check
	if(std::fabs((double)duration/_media_info.video_timescale - _segment_duration) > 0.100)
	{
		logtw("Cmaf Video segment time fail - stream(%s/%s) seq(%u) duration(%.3f) frame(%u) timestamp(%.3f) system(%.3f)",
			  _app_name.CStr(), _stream_name.CStr(), _video_sequence_number,
			  (double)duration/_media_info.video_timescale,
			  _video_chunk_writer->GetSampleCount(),
			  (double)(max_timestamp - _video_start_timestamp)/(_media_info.video_timescale),
			  (GetCurrentMilliseconds() -  _available_start_time)/1000.0);
	}
	else
	{
		logtd("Cmaf Video segment - stream(%s/%s) seq(%u) duration(%.3f) frame(%u) timestamp(%.3f) system(%.3f)",
			  _app_name.CStr(), _stream_name.CStr(), _video_sequence_number,
			  (double)duration/_media_info.video_timescale,
			  _video_chunk_writer->GetSampleCount(),
			  (double)(max_timestamp - _video_start_timestamp)/(_media_info.video_timescale),
			  (GetCurrentMilliseconds() -  _available_start_time)/1000.0);
	}

	auto segment = _video_chunk_writer->GetChunkedSegment();
	_video_chunk_writer->Clear();

	// m4s data save
    SetSegmentData(file_name, duration, _video_chunk_writer->GetStartTimestamp(), segment);

	if(!_video_play_list_update)
	{
		_video_play_list_update = true;

		UpdatePlayList();

		logti("Cmaf Video segment ready completed(ull) - stream(%s/%s)  segment(%ds/%d)",
			  _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

	return true;
}

//====================================================================================================
// Audio Segment Write
// - append frame
// - create segment
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
		{
			_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, GetCurrentAudioFileName(), false);
		}

		_audio_sequence_number = current_number;
	}

	uint64_t duration = max_timestamp - _audio_chunk_writer->GetStartTimestamp();
	auto file_name = GetCurrentAudioFileName();

	// creating chunk data created notify
	if(_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, false);
	}

	// gap 100ms over check
	if(std::fabs((double)duration/_media_info.audio_timescale - _segment_duration) > 0.100)
	{
		logtw("Cmaf Audio segment time fail - stream(%s/%s) seq(%u) duration(%.3f) frame(%u) timestamp(%.3f) system(%.3f)",
			  _app_name.CStr(), _stream_name.CStr(), _audio_sequence_number,
			  (double)duration/_media_info.audio_timescale,
			  _audio_chunk_writer->GetSampleCount(),
			  (double)(max_timestamp - _audio_start_timestamp)/(_media_info.audio_timescale),
			  (GetCurrentMilliseconds() -  _available_start_time)/1000.0);
	}
	else
	{
		logtd("Cmaf Audio segment - stream(%s/%s) seq(%u) duration(%.3f) frame(%u) timestamp(%.3f) system(%.3f)",
			  _app_name.CStr(), _stream_name.CStr(), _audio_sequence_number,
			  (double)duration/_media_info.audio_timescale,
			  _audio_chunk_writer->GetSampleCount(),
			  (double)(max_timestamp - _audio_start_timestamp)/(_media_info.audio_timescale),
			  (GetCurrentMilliseconds() -  _available_start_time)/1000.0);
	}

	auto segment = _audio_chunk_writer->GetChunkedSegment();
	_audio_chunk_writer->Clear();

	// m4s data save
    SetSegmentData(file_name, duration, _audio_chunk_writer->GetStartTimestamp(), segment);

	// playlist format update
	if(!_audio_play_list_update)
	{
		_audio_play_list_update = true;

		UpdatePlayList();

		logti("Cmaf Audio segment ready completed(ull) - stream(%s/%s)  segment(%ds/%d)",
			  _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);

	}

    return true;
}

//====================================================================================================
// PlayList(mpd) Update
// - publishTime, UTCTiming : request time input form GetPlayList function
//
//====================================================================================================
bool CmafPacketyzer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
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
    << "    minimumUpdatePeriod=\"PT30S\"\n" //<< minimumUpdatePeriod << "S\"\n"
    << "    publishTime= %s\n"
    << "    availabilityStartTime=" << MakeUtcMillisecond(_available_start_time).CStr() << "\n"
    << "    timeShiftBufferDepth=\"PT6S\"\n"
    << "    suggestedPresentationDelay=\"PT" << _segment_duration << "S\"\n"
    << "    minBufferTime=\"PT" << _segment_duration << "S\">\n"
    << "<Period id=\"0\" start=\"PT0S\">\n";

    // video listing
    if (_video_sequence_number > 1)
    {
    	double availability_time_offset = _media_info.video_framerate != 0 ?
										  (_segment_duration - (1.0/_media_info.video_framerate)) :
										  _segment_duration;

    	ov::String pixel_aspect_ratio;
		uint32_t resolution_gcd = Gcd(_media_info.video_width, _media_info.video_height);
		if (resolution_gcd != 0)
		{
			pixel_aspect_ratio = ov::String::FormatString("%d:%d", _media_info.video_width / resolution_gcd,
														  _media_info.video_height / resolution_gcd);
		}

        play_list_stream
        << "    <AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" width=\"" << _media_info.video_width
        << "\" height=\"" << _media_info.video_height << "\" par=\"" << pixel_aspect_ratio
        << "\" frameRate=\"" << _media_info.video_framerate
        << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"

        << "        <SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << _media_info.video_timescale
        << "\" duration=\"" << (uint32_t)_segment_duration*_media_info.video_timescale
        << "\" availabilityTimeOffset=\"" << availability_time_offset
        << "\" startNumber=\"1\" initialization=\"" << CMAF_MPD_VIDEO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_VIDEO_SUFFIX << "\" />\n"

        << "        <Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _media_info.video_bitrate  << "\" />\n"

        << "    </AdaptationSet>\n";
    }

    // audio listing
    if (_audio_sequence_number > 1)
    {
		// segment duration - audio one frame duration
		double availability_time_offset = (_media_info.audio_samplerate/1024) != 0 ?
										  _segment_duration - (1.0/((double)_media_info.audio_samplerate/1024)) :
										  _segment_duration;

        play_list_stream
        << "    <AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" "
        << "startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"

        << "        <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" "
        << "value=\"" << _media_info.audio_channels << "\"/>\n"

        << "        <SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << _media_info.audio_timescale
        << "\" duration=\"" << (uint32_t)_segment_duration*_media_info.audio_timescale
		<< "\" availabilityTimeOffset=\"" << availability_time_offset
        << "\" startNumber=\"1\" initialization=\"" << CMAF_MPD_AUDIO_INIT_FILE_NAME
        << "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_AUDIO_SUFFIX << "\" />\n"

        << "        <Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _media_info.audio_samplerate
        << "\" bandwidth=\"" << _media_info.audio_bitrate << "\" />\n"

        << "    </AdaptationSet>\n";
    }

    play_list_stream << "</Period>\n"
    <<"<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=%s/>\n"
    << "</MPD>\n";

    ov::String play_list = play_list_stream.str().c_str();
    SetPlayList(play_list);

    if((_video_play_list_update && _audio_play_list_update) ||			// Audio + video
		(_video_play_list_update && _audio_start_timestamp == 0) ||		// Video Only
		(_audio_play_list_update && _video_start_timestamp == 0))		// Audio Only
	{
		_streaming_start = true;
	}

    return true;
}

//====================================================================================================
// Get Segment
// - video segment
// - audio segment
// - video init
// - audio init
//====================================================================================================
const std::shared_ptr<SegmentData> CmafPacketyzer::GetSegmentData(const ov::String &file_name)
{
    if(!_streaming_start)
        return nullptr;

	const auto &file_type = GetFileType(file_name);

    if (file_type == CmafFileType::VideoSegment)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        auto item = std::find_if(_video_segment_datas.begin(), _video_segment_datas.end(),
                [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

        return (item != _video_segment_datas.end()) ? (*item) : nullptr;
    }
    else if (file_type == CmafFileType::AudioSegment)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        auto item = std::find_if(_audio_segment_datas.begin(), _audio_segment_datas.end(),
                [&](std::shared_ptr<SegmentData> const &value) -> bool
        {
            return value != nullptr ? value->file_name == file_name : false;
        });

		return (item != _audio_segment_datas.end()) ? (*item) : nullptr;
	}
	else if (file_type == CmafFileType::VideoInit)
	{
		return _video_init_file;
	}
	else if (file_type == CmafFileType::AudioInit)
	{
		return _audio_init_file;
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
	const auto &file_type = GetFileType(file_name);

 	if (file_type == CmafFileType::VideoSegment)
    {
        // video segment mutex
        std::unique_lock<std::mutex> lock(_video_segment_guard);

        _video_segment_datas[_current_video_index++] =
        		std::make_shared<SegmentData>(_sequence_number++, file_name, duration, timestamp, data);

        if(_segment_save_count <= _current_video_index)
            _current_video_index = 0;

        _video_sequence_number++;
    }
    else if (file_type == CmafFileType::AudioSegment)
    {
        // audio segment mutex
        std::unique_lock<std::mutex> lock(_audio_segment_guard);

        _audio_segment_datas[_current_audio_index++] =
        		std::make_shared<SegmentData>(_sequence_number++, file_name, duration, timestamp, data);

        if(_segment_save_count <= _current_audio_index)
            _current_audio_index = 0;

        _audio_sequence_number++;
    }

    return true;
}

//====================================================================================================
// Set Segment(override)
// - mpd : "publishTime" and "UTCTiming" : current utc time setting
// - current time : available start time + total timestamp duration + @
//====================================================================================================
bool CmafPacketyzer::GetPlayList(ov::String &play_list)
{
	if(!_streaming_start)
		return false;

	ov::String current_time;

	if(_is_video_time_base)
	{
		current_time = MakeUtcMillisecond(_available_start_time +
				ConvertTimeScale(_video_last_timestamp - _video_start_timestamp,  _media_info.video_timescale, 1000) +
				(GetCurrentTick() - _video_last_tick));
	}
	else
	{
		current_time = MakeUtcMillisecond(_available_start_time +
				ConvertTimeScale(_audio_last_timestamp - _audio_start_timestamp,  _media_info.audio_timescale, 1000) +
				(GetCurrentTick() - _audio_last_tick));
	}

	play_list = ov::String::FormatString(_play_list.CStr(), current_time.CStr(), current_time.CStr());

	return true;
}
