﻿//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/segment_writer/writer.h>

#include "../segment_stream/packetizer/packetizer.h"

class HlsPacketizer : public Packetizer
{
public:
	HlsPacketizer(const ov::String &service_name, const ov::String &app_name, const ov::String &stream_name,
				  uint32_t segment_count, uint32_t segment_duration,
				  const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
				  const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	~HlsPacketizer() override;

	virtual const char *GetPacketizerName() const
	{
		return "HLS";
	}

	//--------------------------------------------------------------------
	// Overriding of Packetizer
	//--------------------------------------------------------------------
	bool ResetPacketizer(uint32_t new_msid) override;

	bool AppendVideoPacket(const std::shared_ptr<const MediaPacket> &media_packet) override;
	bool AppendAudioPacket(const std::shared_ptr<const MediaPacket> &media_packet) override;

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const override;
	bool SetSegmentData(int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms, const std::shared_ptr<const ov::Data> &data);

protected:
	void SetVideoTrack(const std::shared_ptr<MediaTrack> &video_track);
	void SetAudioTrack(const std::shared_ptr<MediaTrack> &audio_track);

	// Returns last_msid
	uint32_t FlushIfNeeded();

	bool WriteSegment(int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms);

	bool UpdatePlayList();

	uint32_t _last_msid = UINT32_MAX;

	std::mutex _flush_mutex;
	bool _need_to_flush = false;

	bool _video_enable = false;
	bool _audio_enable = false;

	uint32_t _sequence_number = 0U;
	uint32_t _discontinuity_sequence_number = 0U;
	uint32_t _last_discontinuity_count = 0U;

	// To convert from timebase to seconds, multiply by these value
	double _video_timebase_expr = 0.0;
	double _audio_timebase_expr = 0.0;

	// To convert from timebase to milliseconds, multiply by these value
	double _video_timebase_expr_ms = 0.0;
	double _audio_timebase_expr_ms = 0.0;

	// To convert from seconds to timebase, multiply by these value
	double _video_timescale = 0.0;
	double _audio_timescale = 0.0;

	// Unit: Timebase of the track
	double _ideal_duration_for_video = 0.0;
	double _ideal_duration_for_audio = 0.0;

	// Unit: millisecond
	int64_t _ideal_duration_for_video_in_ms = 0.0;
	int64_t _ideal_duration_for_audio_in_ms = 0.0;

	// Key: filename
	std::map<ov::String, std::shared_ptr<SegmentItem>> _segment_map;
	std::deque<std::shared_ptr<SegmentItem>> _segment_queue;

	// Since the m4s segment cannot be split exactly to the desired duration, an error is inevitable.
	// As this error results in an incorrect segment index, use the delta to correct the error.
	//
	// delta = <Ideal duration> - <Prev segment duration>
	//
	// So,
	//   delta == 0 means <Ideal duration> == <Average of total segment duration>
	//   delta > 0 means <Ideal duration> > <Average of total segment duration>
	//   delta < 0 means <Ideal duration> < <Average of total segment duration>
	// Unit: Timebase of the track
	double _duration_delta_for_video = 0.0;
	double _duration_delta_for_audio = 0.0;

	// First packet PTS of the segment
	// Unit: Timebase of the track
	int64_t _first_video_pts = -1LL;
	int64_t _first_audio_pts = -1LL;
	// Last received packet PTS
	// Unit: Timebase of the track
	int64_t _last_video_pts = -1LL;
	int64_t _last_audio_pts = -1LL;

	bool _video_ready = false;
	bool _audio_ready = false;

	Writer _ts_writer;

	ov::StopWatch _stat_stop_watch;
};
