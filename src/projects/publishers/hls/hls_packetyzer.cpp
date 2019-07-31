//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_packetyzer.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include "base/ovlibrary/ovlibrary.h"
#include "hls_private.h"
#define HLS_MAX_TEMP_VIDEO_DATA_COUNT (500)

HlsPacketyzer::HlsPacketyzer(const ov::String &app_name,
							 const ov::String &stream_name,
							 PacketyzerStreamType stream_type,
							 const ov::String &segment_prefix,
							 uint32_t segment_count,
							 uint32_t segment_duration,
							 PacketyzerMediaInfo &media_info)
	: Packetyzer(app_name,
				 stream_name,
				 PacketyzerType::Hls,
				 stream_type,
				 segment_prefix,
				 segment_count,
				 (uint32_t)segment_duration,
				 media_info)
{
	_last_video_append_time = time(nullptr);
	_last_audio_append_time = time(nullptr);
	_video_enable = false;
	_audio_enable = false;
	_duration_margin = _segment_duration * 0.1;
}

bool HlsPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
	if (_video_init == false)
	{
		// Wait for first key frame
		if (frame_data->type != PacketyzerFrameType::VideoKeyFrame)
		{
			// Skip the frame
			return true;
		}

		_video_init = true;
	}

	if (frame_data->timescale != _media_info.video_timescale)
	{
		frame_data->timestamp = ConvertTimeScale(frame_data->timestamp, frame_data->timescale, _media_info.video_timescale);
		frame_data->time_offset = ConvertTimeScale(frame_data->time_offset, frame_data->timescale, _media_info.video_timescale);
		frame_data->timescale = _media_info.video_timescale;
	}

	if ((frame_data->type == PacketyzerFrameType::VideoKeyFrame) && (_frame_datas.empty() == false))
	{
		if ((frame_data->timestamp - _frame_datas[0]->timestamp) >=
			((_segment_duration - _duration_margin) * _media_info.video_timescale))
		{
			// Segment Write
			SegmentWrite(_frame_datas[0]->timestamp, frame_data->timestamp - _frame_datas[0]->timestamp);
		}
	}

	// copy
	frame_data->data = frame_data->data->Clone();

	_frame_datas.push_back(frame_data);

	_last_video_append_time = time(nullptr);
	_video_enable = true;

	return true;
}

bool HlsPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data)
{
	if (_audio_init == false)
	{
		_audio_init = true;
	}

	if (frame_data->timescale == _media_info.audio_timescale)
	{
		frame_data->timestamp = ConvertTimeScale(frame_data->timestamp, frame_data->timescale, _media_info.video_timescale);
		frame_data->timescale = _media_info.audio_timescale;
	}

	if (
		((time(nullptr) - _last_video_append_time) >= static_cast<uint32_t>(_segment_duration)) &&
		(_frame_datas.empty() == false))
	{
		if ((frame_data->timestamp - _frame_datas[0]->timestamp) >=
			((_segment_duration - _duration_margin) * _media_info.audio_timescale))
		{
			SegmentWrite(_frame_datas[0]->timestamp, frame_data->timestamp - _frame_datas[0]->timestamp);
		}
	}

	// copy
	frame_data->data = frame_data->data->Clone();

	_frame_datas.push_back(frame_data);

	_last_audio_append_time = time(nullptr);
	_audio_enable = true;

	return true;
}

bool HlsPacketyzer::SegmentWrite(uint64_t start_timestamp, uint64_t duration)
{
	int64_t _first_audio_time_stamp = 0;
	int64_t _first_video_time_stamp = 0;

	auto ts_writer = std::make_unique<TsWriter>(_video_enable, _audio_enable);

	for (auto &frame_data : _frame_datas)
	{
		// Write TS(PES)
		ts_writer->WriteSample(frame_data->type != PacketyzerFrameType::AudioFrame,
							   (frame_data->type == PacketyzerFrameType::AudioFrame) || (frame_data->type == PacketyzerFrameType::VideoKeyFrame),
							   frame_data->timestamp,
							   frame_data->time_offset,
							   frame_data->data);

		if ((_first_audio_time_stamp == 0) && (frame_data->type == PacketyzerFrameType::AudioFrame))
		{
			_first_audio_time_stamp = frame_data->timestamp;
		}
		else if ((_first_video_time_stamp == 0) && (frame_data->type != PacketyzerFrameType::AudioFrame))
		{
			_first_video_time_stamp = frame_data->timestamp;
		}
	}
	_frame_datas.clear();

	if ((_first_audio_time_stamp != 0) && (_first_video_time_stamp != 0))
	{
		logtd("Time difference between A/V: %lldms", (_first_video_time_stamp - _first_audio_time_stamp) / 90);
	}

	auto ts_data = ts_writer->GetDataStream();

	SetSegmentData(ov::String::FormatString("%s_%u.ts", _segment_prefix.CStr(), _sequence_number),
				   duration,
				   start_timestamp,
				   ts_data);

	UpdatePlayList();

	_video_enable = false;
	_audio_enable = false;

	return true;
}

//====================================================================================================
// PlayList(M3U8) 업데이트
// 방송번호_인덱스.TS
//====================================================================================================
bool HlsPacketyzer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
	std::ostringstream m3u8_play_list;
	double max_duration = 0;

	std::vector<std::shared_ptr<SegmentData>> segment_datas;
	Packetyzer::GetVideoPlaySegments(segment_datas);

	for (const auto &segment_data : segment_datas)
	{
		m3u8_play_list << "#EXTINF:" << std::fixed << std::setprecision(3)
					   << (double)(segment_data->duration) / (double)(PACKTYZER_DEFAULT_TIMESCALE) << ",\r\n"
					   << segment_data->file_name.CStr() << "\r\n";

		if (segment_data->duration > max_duration)
		{
			max_duration = segment_data->duration;
		}
	}

	play_list_stream << "#EXTM3U"
					 << "\r\n"
					 << "#EXT-X-MEDIA-SEQUENCE:" << (_sequence_number - 1) << "\r\n"
					 << "#EXT-X-VERSION:3"
					 << "\r\n"
					 << "#EXT-X-ALLOW-CACHE:NO"
					 << "\r\n"
					 << "#EXT-X-TARGETDURATION:" << (int)(max_duration / PACKTYZER_DEFAULT_TIMESCALE) << "\r\n"
					 << m3u8_play_list.str();

	// Playlist 설정
	ov::String play_list = play_list_stream.str().c_str();
	SetPlayList(play_list);

	if ((_stream_type == PacketyzerStreamType::Common) && _streaming_start)
	{
		if (_video_enable == false)
		{
			logtw("There is no HLS video segment for stream [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		}

		if (_audio_enable == false)
		{
			logtw("There is no HLS audio segment for stream [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		}
	}

	return true;
}

const std::shared_ptr<SegmentData> HlsPacketyzer::GetSegmentData(const ov::String &file_name)
{
	if (_streaming_start == false)
	{
		return nullptr;
	}

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_guard);

	auto item = std::find_if(_video_segment_datas.begin(),
							 _video_segment_datas.end(), [&](std::shared_ptr<SegmentData> const &value) -> bool {
								 return value != nullptr ? value->file_name == file_name : false;
							 });

	if (item == _video_segment_datas.end())
	{
		return nullptr;
	}

	return (*item);
}

bool HlsPacketyzer::SetSegmentData(ov::String file_name,
								   uint64_t duration,
								   uint64_t timestamp,
								   std::shared_ptr<ov::Data> &data)
{
	auto segment_data = std::make_shared<SegmentData>(_sequence_number++,
													  file_name,
													  duration,
													  timestamp,
													  data);

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_guard);

	_video_segment_datas[_current_video_index++] = segment_data;

	if (_segment_save_count <= _current_video_index)
	{
		_current_video_index = 0;
	}

	if ((_streaming_start == false) && (_sequence_number > _segment_count))
	{
		_streaming_start = true;
        
		logti("HLS segment is ready for stream [%s/%s], segment duration: %fs, count: %u",
			  _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

	return true;
}
