//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_packetyzer.h"
#include <sstream>
#include <algorithm>
#include <numeric>


#define VIDEO_TRACK_ID	(1)
#define AUDIO_TRACK_ID	(2)

uint32_t Gcd(uint32_t n1, uint32_t n2)
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
// Constructor
//====================================================================================================
DashPacketyzer::DashPacketyzer(std::string 				&segment_prefix,
								PacketyzerStreamType 	stream_type,
								uint32_t 				segment_count,
								uint32_t 				segment_duration,
								PacketyzerMediaInfo 	&media_info) :
								Packetyzer(PacketyzerType::Dash, segment_prefix, stream_type, segment_count, segment_duration, media_info)
{
	_avc_nal_header_size= 0;
	_start_time			= 0;

	_video_frame_datas.clear();
	_audio_frame_datas.clear();

	uint32_t resolution_gcd = Gcd(media_info.video_width, media_info.video_height);

	if(resolution_gcd != 0)
	{
		std::ostringstream 	pixel_aspect_ratio ;
		pixel_aspect_ratio << media_info.video_width / resolution_gcd << ":" << media_info.video_height / resolution_gcd ;
		_pixel_aspect_ratio = pixel_aspect_ratio.str();
	}
}

//====================================================================================================
// Destructor
//====================================================================================================
DashPacketyzer::~DashPacketyzer( )
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
bool DashPacketyzer::VideoInit(std::shared_ptr<std::vector<uint8_t>> &data)
{
    // 패턴 확인
    int current_index    = 0;
    int sps_start_index  = -1;
    int sps_end_index    = -1;
    int pps_start_index  = -1;
    int pps_end_index    = -1;

    // sps/pps parsing
    while(current_index + AVC_NAL_START_PATTERN_SIZE < data->size())
    {
        // 0x00 0x00 0x00 0x01 패턴 체크
        if (data->at(current_index) == 0 && data->at(current_index + 1) == 0 && data->at(current_index + 2) == 0 && data->at(current_index + 3) == 1)
        {
            if (sps_start_index == -1)
            {
                sps_start_index = current_index + AVC_NAL_START_PATTERN_SIZE;
                current_index += AVC_NAL_START_PATTERN_SIZE;
                continue;
            }
            else if (sps_end_index == -1)
            {
                sps_end_index = current_index -1;
                pps_start_index = current_index + AVC_NAL_START_PATTERN_SIZE;
                current_index += AVC_NAL_START_PATTERN_SIZE;
                continue;
            }
            else if (pps_end_index == -1)
            {
                pps_end_index = current_index -1;
                current_index ++;
                continue;
            }
        }
        // 0x00 0x00 0x01 패턴 체크
        else if (data->at(current_index) == 0 && data->at(current_index + 1) == 0 && data->at(current_index + 2) == 1)
        {
            if (sps_start_index == -1)
            {
                sps_start_index = current_index + (AVC_NAL_START_PATTERN_SIZE-1);
                current_index += (AVC_NAL_START_PATTERN_SIZE-1);
                continue;
            }
            else if (sps_end_index == -1)
            {
                sps_end_index = current_index -1;
                pps_start_index = current_index + (AVC_NAL_START_PATTERN_SIZE-1);
                current_index += (AVC_NAL_START_PATTERN_SIZE-1);
                continue;
            }
            else if (pps_end_index == -1)
            {
                pps_end_index = current_index -1;
                current_index ++;
                continue;
            }
        }
        current_index++;
    }

    // parsing result check
    if(sps_start_index < 0 || sps_end_index < 0 || pps_start_index < 0 || pps_end_index < 0)
    {
        return false;
    }

    // SPS/PPS 저장
	auto avc_sps = std::make_shared<std::vector<uint8_t>>(data->begin() + sps_start_index, data->begin() + sps_end_index + 1);
	auto avc_pps = std::make_shared<std::vector<uint8_t>>(data->begin() + pps_start_index, data->begin() + pps_end_index + 1);

	// Video init m4s 생성(메모리)
    auto writer = std::make_unique<M4sInitWriter>(M4sMediaType::VideoMediaType,
                                                    1024,
                                                    _segment_duration*_media_info.video_timescale,
                                                    _media_info.video_timescale,
                                                    VIDEO_TRACK_ID,
                                                    _media_info.video_width,
                                                    _media_info.video_height,
                                                    avc_sps, 
													avc_pps,
													_media_info.audio_channels, 
													16, 
													_media_info.audio_samplerate);

    if(writer->CreateData() <= 0)
    {
        printf("DashPacketyzer::VideoInit - InitWrite Fail\n");
        return false;
    }

    _avc_nal_header_size = avc_sps->size() + avc_pps->size() + AVC_NAL_START_PATTERN_SIZE*3;
	auto data_stream = writer->GetDataStream();

	// Video init m4s Save
	SetSegmentData(SegmentDataType::Mp4Video, 0, "video_init.m4s", 0, 0, data_stream, false);

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
													_segment_duration*_media_info.audio_timescale,
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
		printf("DashPacketyzer::VideoInit - InitWrite Fail\n");
		return false;
	}

	auto data_stream = writer->GetDataStream();

    // Audio init m4s Save
	SetSegmentData(SegmentDataType::Mp4Audio, 0, "audio_init.m4s", 0, 0, data_stream, false);

    _audio_init = true;

    return true;
}

//====================================================================================================
// Video Frame Append
// - Video(Audio) Segment m4s Create
//====================================================================================================
bool DashPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
    // Init Check
    if(!_video_init)
    {
        if((frame_data->type != PacketyzerFrameType::VideoIFrame) || !VideoInit(frame_data->data)) return false;
    }

    // Fragment Check
    // - KeyFrame ~ KeyFrame 전까지
    if(frame_data->type == PacketyzerFrameType::VideoIFrame && !_video_frame_datas.empty())
    {
		// druation check(I-Frame Start)
        if((frame_data->timestamp - _video_frame_datas[0]->timestamp) > ((((double)_segment_duration) * 0.87) * _media_info.video_timescale)) // I-Frame 관련 threshold 87% 한도 내에서 처리
        {
			// Video Segment Write
            VideoSegmentWrite(frame_data->timestamp);

			// Audio Segment Write 
			if (_stream_type != PacketyzerStreamType::VideoOnly&& _audio_init)
			{
				AudioSegmentWrite(ConvertTimeScale(frame_data->timestamp, _media_info.video_timescale, _media_info.audio_timescale));
			}

			// Mpd Update
			UpdatePlayList(true);
        }
    }

    _video_frame_datas.push_back(frame_data);

    return true;
}

//====================================================================================================
// Audio Frame Append
// - Audio Segment m4s Create
//====================================================================================================
bool DashPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData>  &frame_data)
{
    // Init Check
    if(!_audio_init)
    {
        if(!AudioInit()) return false;
    }

	if (!_audio_frame_datas.empty() && _stream_type == PacketyzerStreamType::AudioOnly)
	{
		// duration check
		if ((frame_data->timestamp - _audio_frame_datas[0]->timestamp) > (_segment_duration * _media_info.audio_timescale))
		{
			AudioSegmentWrite(frame_data->timestamp);

			// Mpd Update
			UpdatePlayList(false);
		}
	}

	_audio_frame_datas.push_back(frame_data);

    return true;
}

//====================================================================================================
// Video Segment Write
// - Duration/Key Frame 확인 이후 이전 데이터 까지 생성
//====================================================================================================
bool DashPacketyzer::VideoSegmentWrite(uint64_t last_timestamp)
{
	uint64_t	duration		= 0;
	uint64_t	start_timestamp = 0;
	bool		start_check		= false;
	std::vector<std::shared_ptr<FragmentSampleData>> sample_datas;

	while (!_video_frame_datas.empty())
	{
		auto frame_data = _video_frame_datas.front();
		_video_frame_datas.pop_front();

		if(!start_check)
		{
			start_check = true;
			start_timestamp = frame_data->timestamp;
		}
		
		if (_video_frame_datas.empty())	duration = last_timestamp - frame_data->timestamp;
		else							duration = _video_frame_datas.front()->timestamp - frame_data->timestamp;

		auto data = std::make_shared<std::vector<uint8_t>>((frame_data->type == PacketyzerFrameType::VideoIFrame) ? (frame_data->data->begin() + _avc_nal_header_size) : (frame_data->data->begin() + AVC_NAL_START_PATTERN_SIZE), frame_data->data->end());
		auto sample_data = std::make_shared<FragmentSampleData>(duration, frame_data->type == PacketyzerFrameType::VideoIFrame ? 0X02000000 : 0X01010000, frame_data->time_offset, data);

		sample_datas.push_back(sample_data);
	}
	_video_frame_datas.clear();

    // Fragment 쓰기
    auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::VideoMediaType, 4096, _video_sequence_number, VIDEO_TRACK_ID, start_timestamp, sample_datas);

	fragment_writer->CreateData();
    auto data_stream = fragment_writer->GetDataStream();

    std::ostringstream	file_name;
	file_name << _segment_prefix << "_" << start_timestamp << "_video.m4s";

    // m3s 데이터 저장
    SetSegmentData(SegmentDataType::Mp4Video, _sequence_number, file_name.str(), last_timestamp - start_timestamp, start_timestamp, data_stream, true);

    _sequence_number++;
	_video_sequence_number++; 

    return true;
}

//====================================================================================================
// Audio Segment Write
// - 비디오 Segment 생성이후 생성 or Audio Only 에서 생성
//====================================================================================================
bool DashPacketyzer::AudioSegmentWrite(uint64_t last_timestamp)
{
	uint32_t	duration		= 0; 
	uint64_t	start_timestamp	= 0; 
	bool		start_check		= false;
	uint64_t	end_timestamp	= 0; 
	std::vector<std::shared_ptr<FragmentSampleData>> sample_datas;

	while (!_audio_frame_datas.empty())
	{
		auto frame_data = _audio_frame_datas.front();
		
		if (!start_check)
		{
			start_check = true;
			start_timestamp = frame_data->timestamp;
		}

		if (_audio_frame_datas.size() <= 1 || frame_data->timestamp >= last_timestamp)
		{
			break; 
		}
		
		// pop 
		_audio_frame_datas.pop_front(); 
		
		duration = (uint32_t)(_audio_frame_datas.front()->timestamp - frame_data->timestamp);
		end_timestamp = _audio_frame_datas.front()->timestamp;

		auto data = std::make_shared<std::vector<uint8_t>>((frame_data->data->begin() + ADTS_HEADER_SIZE), frame_data->data->end());
		auto sample_data = std::make_shared<FragmentSampleData>(duration, 0, 0, data);

		sample_datas.push_back(sample_data);
	}

	// Fragment 쓰기
	auto fragment_writer = std::make_unique<M4sFragmentWriter>(M4sMediaType::AudioMediaType, 4096, _audio_sequence_number, AUDIO_TRACK_ID, start_timestamp, sample_datas);

	fragment_writer->CreateData();
	auto data_stream = fragment_writer->GetDataStream();

	std::ostringstream	file_name;
	
	file_name << _segment_prefix << "_" << start_timestamp << "_audio.m4s";

	// m3s 데이터 저장
	SetSegmentData(SegmentDataType::Mp4Audio, _sequence_number, file_name.str(), end_timestamp - start_timestamp, start_timestamp, data_stream, true);

	_sequence_number++;
	_audio_sequence_number++; 

	return true;
}

std::string MakeUtcTimeString(time_t value)
{
    std::tm* now_tm= gmtime(&value);
    char buf[42];
    strftime(buf, 42, "\"%Y-%m-%dT%TZ\"", now_tm);
    return buf;
}

//====================================================================================================
// PlayList(mpd) 업데이트
// - 차후 자동 인덱싱 사용시 참고 : LSN = floor(now - (availabilityStartTime + PST) / segmentDuration + startNumber - 1)
//====================================================================================================
bool DashPacketyzer::UpdatePlayList(bool video_update)
{
	time_t 				update_time = time(nullptr);
	std::ostringstream 	play_list;
	std::ostringstream 	video_segment_urls;
	std::ostringstream 	audio_segment_urls;
    uint64_t			video_total_duration		= 0;
	uint64_t			video_min_duration			= 0;
	bool				video_first_segment_check	= false;
	
	uint64_t			audio_total_duration 		= 0;
	uint64_t			audio_min_duration 			= 0;
	bool				audio_first_segment_check 	= false;

    if (_start_time == 0)
    {
        _start_time = time(nullptr);
    }


	std::unique_lock<std::mutex> segment_datas_lock(_segment_datas_mutex);

	// Video Segment Listing
	for (int index = std::max((int)(_video_segment_indexer.size() - _segment_count), 0); index < _video_segment_indexer.size(); index++)
	{
		auto item = _segment_datas.find(_video_segment_indexer[index]);
		if (item == _segment_datas.end()) continue; 
		
		// Timeline Setting
		if (!video_first_segment_check)	video_segment_urls << "\t\t\t\t" << "<S t=\"" << item->second->timestamp << "\" d=\"" << item->second->duration << "\"/>\n";
		else							video_segment_urls << "\t\t\t\t" << "<S d=\"" << item->second->duration << "\"/>\n";
		video_first_segment_check = true;
		
		// 전체 duration 
		video_total_duration += item->second->duration;

		// 최소 duration 
		if (video_min_duration == 0 || video_min_duration > item->second->duration) video_min_duration = item->second->duration;
	}

	// Audio Segment Listing 
	for (int index = std::max((int)(_audio_segment_indexer.size() - _segment_count), 0); index < _audio_segment_indexer.size(); index++)
	{
		auto item = _segment_datas.find(_audio_segment_indexer[index]);
		if (item == _segment_datas.end()) continue;

		// Timeline Setting
		if (audio_first_segment_check) 	audio_segment_urls << "\t\t\t\t<S d=\"" << item->second->duration << "\"/>\n";
		else 							audio_segment_urls << "\t\t\t\t<S t=\"" << item->second->timestamp << "\" d=\"" << item->second->duration << "\"/>\n";

		audio_first_segment_check = true;

		// 전체 duration 
		audio_total_duration += item->second->duration;

		// 최소 duration 
		if (audio_min_duration == 0 || audio_min_duration > item->second->duration) audio_min_duration = item->second->duration;
	}
	
	segment_datas_lock.unlock();
	
    play_list       << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
                    << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
                    << "    xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
                    << "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
                    << "    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
                    << "    type=\"dynamic\"\n"
                    << "    minimumUpdatePeriod=\"PT" << video_min_duration / _media_info.video_timescale << "S\"\n"
                    << "    publishTime=" << MakeUtcTimeString(update_time) << "\n"
                    << "    availabilityStartTime=" << MakeUtcTimeString(_start_time) << "\n"
                    << "    timeShiftBufferDepth=\"PT" << video_total_duration / _media_info.video_timescale << "S\"\n"
                    << "    suggestedPresentationDelay=\"PT" << _segment_count / 2 * _segment_duration << "S\"\n"
                    << "    minBufferTime=\"PT" << _segment_duration / 2 + 1 << "S\">\n"
                    << "<Period id=\"0\" start=\"PT0.0S\">\n";
	
	// video listing 
	if (!video_segment_urls.str().empty())
	{
		play_list   << "\t<AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" width=\"" << _media_info.video_width << "\" height=\"" << _media_info.video_height
		            << "\" par=\"" << _pixel_aspect_ratio << "\" frameRate=\"" << _media_info.video_framerate
		            << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">" << "\n"
                    << "\t\t<SegmentTemplate timescale=\"" << _media_info.video_timescale << "\" initialization=\"video_init.m4s\" media=\"" << _segment_prefix << "_$Time$_video.m4s\">\n"
                    << "\t\t\t<SegmentTimeline>\n"
                    << video_segment_urls.str()
                    << "\t\t\t</SegmentTimeline>\n"
                    << "\t\t</SegmentTemplate>\n"
                    << "\t\t<Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _media_info.video_bitrate << "\" />\n"
                    << "\t</AdaptationSet>\n";
	}

	// audio listing
	if (!audio_segment_urls.str().empty())
	{
		play_list   << "\t<AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
                    << "\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"" << _media_info.audio_channels << "\"/>\n"
                    << "\t\t<SegmentTemplate timescale=\"" << _media_info.audio_timescale << "\" initialization=\"audio_init.m4s\" media=\"" << _segment_prefix << "_$Time$_audio.m4s\">\n"
                    << "\t\t\t<SegmentTimeline>\n"
                    << audio_segment_urls.str()
                    << "\t\t\t</SegmentTimeline>\n"
                    << "\t\t</SegmentTemplate>\n"
                    << "\t\t<Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _media_info.audio_samplerate << "\" bandwidth=\"" << _media_info.audio_bitrate << "\" />\n"
                    << "\t</AdaptationSet>\n";
	}

    play_list       << "</Period>\n"
                    << "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=" << MakeUtcTimeString(update_time) << "/>\n"
                    << "</MPD>\n";
	std::string play_list_data = play_list.str();
	SetPlayList(play_list_data);

    return true;
}

