//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "packetyzer.h"
#include <sys/time.h>
#include <algorithm>
#include <sstream>
#include "../segment_stream_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
Packetyzer::Packetyzer(const ov::String &app_name,
					   const ov::String &stream_name,
					   PacketyzerType packetyzer_type,
					   PacketyzerStreamType stream_type,
					   const ov::String &segment_prefix,
					   uint32_t segment_count,
					   uint32_t segment_duration,
					   PacketyzerMediaInfo &media_info)
{
	_app_name = app_name;
	_stream_name = stream_name;
	_packetyzer_type = packetyzer_type;
	_stream_type = stream_type;
	_segment_prefix = segment_prefix;
	_segment_count = segment_count;
	_segment_save_count = segment_count * 10;
	_segment_duration = segment_duration;

	_media_info = media_info;
	_sequence_number = 1;

	_video_init = false;
	_audio_init = false;

	_streaming_start = false;

	// init nullptr
	for (uint32_t index = 0; index < _segment_save_count; index++)
	{
		_video_segment_datas.push_back(nullptr);

		// only dash/cmaf
		if (_packetyzer_type == PacketyzerType::Dash || _packetyzer_type == PacketyzerType::Cmaf)
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
	struct timespec now;

	clock_gettime(CLOCK_REALTIME, &now);

	double milliseconds = now.tv_sec * 1000LL + now.tv_nsec / 1000000;  // calculate milliseconds
	return milliseconds;
}

//====================================================================================================
// Packetyzer(Util)
// - 1/1000
//====================================================================================================
double Packetyzer::GetCurrentTick()
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	double milliseconds = now.tv_sec * 1000LL + now.tv_nsec / 1000000;  // calculate milliseconds
	return milliseconds;
}

//====================================================================================================
// MakeUtcSecond(second)
//====================================================================================================
ov::String Packetyzer::MakeUtcSecond(time_t value)
{
	std::tm *now_tm = gmtime(&value);
	char buffer[42];

	strftime(buffer, sizeof(buffer), "\"%Y-%m-%dT%TZ\"", now_tm);

	return buffer;
}

//====================================================================================================
// MakeUtcMillisecond(mille second)
//====================================================================================================
ov::String Packetyzer::MakeUtcMillisecond(double value)
{
	time_t time_vaule = (time_t)value / 1000;
	std::tm *now_tm = gmtime(&time_vaule);
	char buffer[42];

	strftime(buffer, sizeof(buffer), "%Y-%m-%dT%T", now_tm);

	return ov::String::FormatString("\"%s.%uZ\"", buffer, (uint32_t)value % 1000);
}

uint64_t Packetyzer::ConvertTimeScale(uint64_t time, uint32_t from_timescale, uint32_t to_timescale)
{
	if (from_timescale == 0)
	{
		return 0;
	}

	double ratio = (double)to_timescale / (double)from_timescale;

	return (uint64_t)((double)time * ratio);
}

//====================================================================================================
// PlayList
// - thread safe
//====================================================================================================
void Packetyzer::SetPlayList(ov::String &play_list)
{
	// playlist mutex
	std::unique_lock<std::mutex> lock(_play_list_guard);
	_play_list = play_list;
}

//====================================================================================================
// PlayList
// - thread safe
//====================================================================================================
bool Packetyzer::GetPlayList(ov::String &play_list)
{
	if (!_streaming_start)
		return false;

	// playlist mutex
	std::unique_lock<std::mutex> lock(_play_list_guard);

	play_list = _play_list;

	return true;
}

//====================================================================================================
// Last (segment count) Video(or Video+Audio) Segments
// - thread safe
//====================================================================================================
bool Packetyzer::GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas)
{
	uint32_t begin_index = (_current_video_index >= _segment_count) ? (_current_video_index - _segment_count) : (_segment_save_count - (_segment_count - _current_video_index));

	uint32_t end_index = (begin_index <= (_segment_save_count - _segment_count)) ? (begin_index + _segment_count) - 1 : (_segment_count - (_segment_save_count - begin_index)) - 1;

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_guard);

	if (begin_index <= end_index)
	{
		for (auto item = _video_segment_datas.begin() + begin_index; item <= _video_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
				return true;

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _video_segment_datas.begin() + begin_index; item < _video_segment_datas.end(); item++)
		{
			if (*item == nullptr)
				return true;

			segment_datas.push_back(*item);
		}

		for (auto item = _video_segment_datas.begin(); item <= _video_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
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
	uint32_t begin_index = (_current_audio_index >= _segment_count) ? (_current_audio_index - _segment_count) : (_segment_save_count - (_segment_count - _current_audio_index));

	uint32_t end_index = (begin_index <= (_segment_save_count - _segment_count)) ? (begin_index + _segment_count) - 1 : (_segment_count - (_segment_save_count - begin_index)) - 1;

	// audio segment mutex
	std::unique_lock<std::mutex> lock(_audio_segment_guard);

	if (begin_index <= end_index)
	{
		for (auto item = _audio_segment_datas.begin() + begin_index; item <= _audio_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
				return true;

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _audio_segment_datas.begin() + begin_index; item < _audio_segment_datas.end(); item++)
		{
			if (*item == nullptr)
				return true;

			segment_datas.push_back(*item);
		}
		for (auto item = _audio_segment_datas.begin(); item <= _audio_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
				return true;

			segment_datas.push_back(*item);
		}
	}

	return true;
}