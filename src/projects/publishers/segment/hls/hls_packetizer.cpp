//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_packetizer.h"
#include "hls_private.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

#include <base/ovlibrary/ovlibrary.h>
#include <publishers/segment/segment_stream/packetizer/packetizer_define.h>

#define HLS_MAX_TEMP_VIDEO_DATA_COUNT (500)

const cmn::Timebase DEFAULT_TIMEBASE(1, PACKTYZER_DEFAULT_TIMESCALE);

HlsPacketizer::HlsPacketizer(const ov::String &app_name,
							 const ov::String &stream_name,
							 PacketizerStreamType stream_type,
							 const ov::String &segment_prefix,
							 uint32_t segment_count,
							 uint32_t segment_duration,
							 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track)
	: Packetizer(app_name,
				 stream_name,
				 PacketizerType::Hls,
				 stream_type,
				 segment_prefix,
				 segment_count,
				 (uint32_t)segment_duration,
				 video_track, audio_track)
{
	_last_video_append_time = ::time(nullptr);
	_last_audio_append_time = ::time(nullptr);

	_video_enable = false;
	_audio_enable = false;

	_duration_margin = _segment_duration * 0.1;

	_stat_stop_watch.Start();
}

bool HlsPacketizer::AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame_data)
{
	if (_video_track == nullptr)
	{
		// Video frame is appended since video track is not enable
		OV_ASSERT2(false);

		return false;
	}

	if (_video_init == false)
	{
		// Wait for first key frame
		if (frame_data->type != PacketizerFrameType::VideoKeyFrame)
		{
			// Skip the frame
			return true;
		}

		_video_init = true;
	}

	if (frame_data->timebase != DEFAULT_TIMEBASE)
	{
		frame_data->pts = ConvertTimeScale(frame_data->pts, frame_data->timebase, DEFAULT_TIMEBASE);
		frame_data->dts = ConvertTimeScale(frame_data->dts, frame_data->timebase, DEFAULT_TIMEBASE);
		frame_data->timebase = DEFAULT_TIMEBASE;
	}

	if ((frame_data->type == PacketizerFrameType::VideoKeyFrame) && (_frame_datas.empty() == false))
	{
		if ((frame_data->pts - _frame_datas[0]->pts) >=
			((_segment_duration - _duration_margin) * DEFAULT_TIMEBASE.GetTimescale()))
		{
			// Segment Write
			SegmentWrite(_frame_datas[0]->pts, frame_data->pts - _frame_datas[0]->pts);
		}
	}

	// copy
	frame_data->data = frame_data->data->Clone();

	_frame_datas.push_back(frame_data);

	_last_video_append_time = time(nullptr);
	_video_enable = true;

	return true;
}

bool HlsPacketizer::AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame_data)
{
	if (_audio_init == false)
	{
		_audio_init = true;
	}

	if (frame_data->timebase != DEFAULT_TIMEBASE)
	{
		frame_data->pts = ConvertTimeScale(frame_data->pts, frame_data->timebase, DEFAULT_TIMEBASE);
		frame_data->dts = ConvertTimeScale(frame_data->dts, frame_data->timebase, DEFAULT_TIMEBASE);
		frame_data->timebase = DEFAULT_TIMEBASE;
	}

	if (
		((time(nullptr) - _last_video_append_time) >= static_cast<uint32_t>(_segment_duration)) &&
		(_frame_datas.empty() == false))
	{
		if ((frame_data->pts - _frame_datas[0]->pts) >=
			((_segment_duration - _duration_margin) * DEFAULT_TIMEBASE.GetTimescale()))
		{
			SegmentWrite(_frame_datas[0]->pts, frame_data->pts - _frame_datas[0]->pts);
		}
	}

	// copy
	frame_data->data = frame_data->data->Clone();

	_frame_datas.push_back(frame_data);

	_last_audio_append_time = time(nullptr);
	_audio_enable = true;

	return true;
}

bool HlsPacketizer::SegmentWrite(int64_t start_timestamp, uint64_t duration)
{
	int64_t _first_audio_time_stamp = 0;
	int64_t _first_video_time_stamp = 0;

	auto ts_writer = std::make_shared<TsWriter>(_video_enable, _audio_enable);

	for (auto &frame_data : _frame_datas)
	{
		// Write TS(PES)
		ts_writer->WriteSample(frame_data->type != PacketizerFrameType::AudioFrame,
							   (frame_data->type == PacketizerFrameType::AudioFrame) || (frame_data->type == PacketizerFrameType::VideoKeyFrame),
							   frame_data->pts, frame_data->dts,
							   frame_data->data);

		if ((_first_audio_time_stamp == 0) && (frame_data->type == PacketizerFrameType::AudioFrame))
		{
			_first_audio_time_stamp = frame_data->pts;
		}
		else if ((_first_video_time_stamp == 0) && (frame_data->type != PacketizerFrameType::AudioFrame))
		{
			_first_video_time_stamp = frame_data->pts;
		}
	}
	_frame_datas.clear();

	if(_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		if ((_video_track != nullptr) && (_audio_track != nullptr))
		{
			logts("[%s/%s] HLS A-V Sync: %lld (A: %lld, V: %lld)",
				_app_name.CStr(), _stream_name.CStr(),
				(_first_audio_time_stamp - _first_video_time_stamp) / 90, _first_audio_time_stamp / 90, _first_video_time_stamp / 90);
		}
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
bool HlsPacketizer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
	std::ostringstream m3u8_play_list;
	double max_duration = 0;

	std::vector<std::shared_ptr<SegmentData>> segment_datas;
	Packetizer::GetVideoPlaySegments(segment_datas);

	for (const auto &segment_data : segment_datas)
	{
		m3u8_play_list << "#EXTINF:" << std::fixed << std::setprecision(0)
					   << (double)(segment_data->duration) / (double)(PACKTYZER_DEFAULT_TIMESCALE) << "\r\n"
					   << segment_data->file_name.CStr() << "\r\n";

		if (segment_data->duration > max_duration)
		{
			max_duration = segment_data->duration;
		}
	}

	play_list_stream << "#EXTM3U\r\n"
					 << "#EXT-X-VERSION:3\r\n"
					 << "#EXT-X-MEDIA-SEQUENCE:" << (_sequence_number - 1) << "\r\n"
					 << "#EXT-X-ALLOW-CACHE:NO\r\n"
					 << "#EXT-X-TARGETDURATION:" << std::fixed << std::setprecision(0) << (double)(max_duration) / PACKTYZER_DEFAULT_TIMESCALE << "\r\n"
					 << m3u8_play_list.str();

	// Playlist 설정
	ov::String play_list = play_list_stream.str().c_str();
	SetPlayList(play_list);

	if ((_stream_type == PacketizerStreamType::Common) && IsReadyForStreaming())
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

const std::shared_ptr<SegmentData> HlsPacketizer::GetSegmentData(const ov::String &file_name)
{
	if (IsReadyForStreaming() == false)
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

bool HlsPacketizer::SetSegmentData(ov::String file_name,
								   uint64_t duration,
								   int64_t timestamp,
								   std::shared_ptr<ov::Data> &data)
{
	auto segment_data = std::make_shared<SegmentData>(
		cmn::MediaType::Unknown,
		_sequence_number++,
		file_name,
		timestamp,
		duration,
		data);

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_guard);

	_video_segment_datas[_current_video_index++] = segment_data;

	if (_segment_save_count <= _current_video_index)
	{
		_current_video_index = 0;
	}

	if ((IsReadyForStreaming() == false) && (_sequence_number > _segment_count))
	{
		SetReadyForStreaming();

		logti("HLS segment is ready for stream [%s/%s], segment duration: %fs, count: %u",
			  _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

	return true;
}
