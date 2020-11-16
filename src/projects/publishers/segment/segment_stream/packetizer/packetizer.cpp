//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "packetizer.h"
#include "../segment_stream_private.h"

#include <sys/time.h>
#include <algorithm>
#include <sstream>

Packetizer::Packetizer(const ov::String &app_name, const ov::String &stream_name,
					   PacketizerType packetizer_type, PacketizerStreamType stream_type,
					   const ov::String &segment_prefix,
					   uint32_t segment_count, uint32_t segment_duration,
					   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track)
	: _app_name(app_name), _stream_name(stream_name),
	  _packetizer_type(packetizer_type),
	  _segment_prefix(segment_prefix),
	  _stream_type(stream_type),

	  _segment_count(segment_count),
	  _segment_save_count(segment_count * 5),
	  _segment_duration(segment_duration),

	  _video_track(video_track),
	  _audio_track(audio_track)
{
	for (uint32_t index = 0; index < _segment_save_count; index++)
	{
		_video_segment_datas.push_back(nullptr);

		// Only for dash
		if (_packetizer_type == PacketizerType::Dash)
		{
			_audio_segment_datas.push_back(nullptr);
		}
	}
}

uint32_t Packetizer::Gcd(uint32_t n1, uint32_t n2)
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

int64_t Packetizer::GetCurrentMilliseconds()
{
	struct timespec now;

	::clock_gettime(CLOCK_REALTIME, &now);

	return now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
}

int64_t Packetizer::GetCurrentTick()
{
	struct timespec now;

	::clock_gettime(CLOCK_MONOTONIC, &now);

	return now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
}

int64_t Packetizer::GetTimestampInMs()
{
	auto current = std::chrono::system_clock::now().time_since_epoch();

	return std::chrono::duration_cast<std::chrono::milliseconds>(current).count();
}

ov::String Packetizer::MakeUtcSecond(time_t value)
{
	std::tm *now_tm = ::gmtime(&value);
	char buffer[42];

	::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%TZ", now_tm);

	return buffer;
}

ov::String Packetizer::MakeUtcMillisecond(int64_t value)
{
	if (value == -1)
	{
		value = GetTimestampInMs();
	}

	ov::String result;

	// YYYY-MM-DDTHH:II:SS.sssZ
	// 012345678901234567890123
	result.SetCapacity(24);

	time_t time_value = static_cast<time_t>(value) / 1000;
	std::tm *now_tm = ::gmtime(&time_value);
	char *buffer = result.GetBuffer();

	// YYYY-MM-DDTHH:II:SS.sssZ
	// ~~~~~~~~~~~~~~~~~~~
	auto length = ::strftime(buffer, result.GetCapacity(), "%Y-%m-%dT%T", now_tm);

	if (result.SetLength(length))
	{
		// YYYY-MM-DDTHH:II:SS.sssZ
		//                    ~~~~~
		result.AppendFormat(".%03uZ", static_cast<uint32_t>(value % 1000LL));
		return result;
	}

	return "";
}

uint64_t Packetizer::ConvertTimeScale(uint64_t time, const cmn::Timebase &from_timebase, const cmn::Timebase &to_timebase)
{
	if (from_timebase.GetExpr() == 0.0)
	{
		return 0;
	}

	double ratio = from_timebase.GetExpr() / to_timebase.GetExpr();

	return (uint64_t)((double)time * ratio);
}

void Packetizer::SetPlayList(ov::String &play_list)
{
	std::unique_lock<std::mutex> lock(_play_list_guard);

	_play_list = play_list;
}

bool Packetizer::IsReadyForStreaming() const noexcept
{
	return _streaming_start;
}

void Packetizer::SetReadyForStreaming() noexcept
{
	_streaming_start = true;
}

bool Packetizer::GetPlayList(ov::String &play_list)
{
	if (IsReadyForStreaming() == false)
	{
		return false;
	}

	std::unique_lock<std::mutex> lock(_play_list_guard);

	play_list = _play_list;

	return true;
}

bool Packetizer::GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas)
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
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _video_segment_datas.begin() + begin_index; item < _video_segment_datas.end(); item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}

		for (auto item = _video_segment_datas.begin(); item <= _video_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}

	return true;
}

bool Packetizer::GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas)
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
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _audio_segment_datas.begin() + begin_index; item < _audio_segment_datas.end(); item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
		for (auto item = _audio_segment_datas.begin(); item <= _audio_segment_datas.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}

	return true;
}