//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/application.h>
#include <base/info/media_track.h>
#include <base/ovlibrary/ovlibrary.h>

#include "packetizer_define.h"

class Packetizer
{
public:
	Packetizer(const ov::String &app_name, const ov::String &stream_name,
			   PacketizerType packetizer_type, PacketizerStreamType stream_type,
			   const ov::String &segment_prefix,
			   uint32_t segment_count, uint32_t segment_duration,
			   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track);

	virtual ~Packetizer() = default;

	virtual const char *GetPacketizerName() const = 0;

	virtual bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame) = 0;
	virtual bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame) = 0;

	virtual const std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) = 0;
	virtual bool SetSegmentData(ov::String file_name, uint64_t duration, int64_t timestamp, std::shared_ptr<ov::Data> &data) = 0;

	// Convert timescale of "time" to "to_timescale" from "from_timescale"
	//
	// For example:
	//   +--------+---------+--------+-----------+
	//   | time   | from_ts | to_ts  | result    |
	//   +--------+---------+--------+-----------+
	//   | 1000   | 2       | 10     | 5000      |
	//   | 1000   | 5       | 10     | 2000      |
	//   | 1000   | 10      | 2      | 200       |
	//   +--------+---------+--------+-----------+
	static uint64_t ConvertTimeScale(uint64_t time, const cmn::Timebase &from_timebase, const cmn::Timebase &to_timebase);

	void SetPlayList(ov::String &play_list);

	virtual bool IsReadyForStreaming() const noexcept;
	virtual bool GetPlayList(ov::String &play_list);

	bool GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);
	bool GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);

	static uint32_t Gcd(uint32_t n1, uint32_t n2);
	static int64_t GetTimestampInMs();
	static ov::String MakeUtcSecond(time_t value);
	static ov::String MakeUtcMillisecond(int64_t value = -1LL);
	static int64_t GetCurrentMilliseconds();
	static int64_t GetCurrentTick();

protected:
	virtual void SetReadyForStreaming() noexcept;

	ov::String _app_name;
	ov::String _stream_name;
	PacketizerType _packetizer_type;
	ov::String _segment_prefix;
	PacketizerStreamType _stream_type;

	uint32_t _segment_count = 0U;
	uint32_t _segment_save_count = 0U;
	// Duration in second
	double _segment_duration = 0.0;

	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;

	uint32_t _sequence_number = 1U;
	bool _streaming_start = false;
	ov::String _play_list;

	bool _video_init = false;
	bool _audio_init = false;

	uint32_t _current_video_index = 0U;
	uint32_t _current_audio_index = 0U;

	std::vector<std::shared_ptr<SegmentData>> _video_segment_datas;  // m4s : video , ts : video+audio
	std::vector<std::shared_ptr<SegmentData>> _audio_segment_datas;  // m4s : audio

	std::mutex _video_segment_guard;
	std::mutex _audio_segment_guard;
	std::mutex _play_list_guard;
};
