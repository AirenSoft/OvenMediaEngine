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
#include <publishers/segment/segment_stream/packetizer/m4s_init_writer.h>
#include <publishers/segment/segment_stream/packetizer/m4s_segment_writer.h>
#include <publishers/segment/segment_stream/packetizer/packetizer.h>

enum class DashFileType : int32_t
{
	Unknown,
	PlayList,
	VideoInit,
	AudioInit,
	VideoSegment,
	AudioSegment,
};

class DashPacketizer : public Packetizer
{
public:
	DashPacketizer(const ov::String &service_name, const ov::String &app_name, const ov::String &stream_name,
				   uint32_t segment_count, uint32_t segment_duration,
				   const ov::String &utc_timing_scheme, const ov::String &utc_timing_value,
				   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
				   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	~DashPacketizer() override;

	virtual const char *GetPacketizerName() const
	{
		return "DASH";
	}

	static DashFileType GetFileType(const ov::String &file_name);

	//--------------------------------------------------------------------
	// Overriding of Packetizer
	//--------------------------------------------------------------------
	bool ResetPacketizer(uint32_t new_msid) override;

	bool AppendVideoPacket(const std::shared_ptr<const MediaPacket> &media_packet) override;
	bool AppendAudioPacket(const std::shared_ptr<const MediaPacket> &media_packet) override;

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const override;
	bool SetSegmentData(Writer &writer, int64_t timestamp);

protected:
	using DataCallback = std::function<void(const std::shared_ptr<const SampleData> &data, bool new_segment_written)>;

	void SetVideoTrack(const std::shared_ptr<MediaTrack> &video_track);
	void SetAudioTrack(const std::shared_ptr<MediaTrack> &audio_track);

	// Unit: timebase
	ov::String GetFileName(int segment_index, cmn::MediaType media_type) const;

	bool PrepareVideoInitIfNeeded();
	bool PrepareAudioInitIfNeeded();

	bool WriteVideoSegment();
	bool WriteAudioSegment();

	bool GetSegmentInfos(ov::String *video_urls, ov::String *audio_urls, double *time_shift_buffer_depth, double *minimum_update_period, size_t segment_count);

	virtual bool UpdatePlayList();

	void SetReadyForStreaming() noexcept override;

protected:
	ov::String _utc_timing_scheme;
	ov::String _utc_timing_value;

	bool _video_enable = false;
	bool _audio_enable = false;

	uint32_t _video_sequence_number = 0U;
	uint32_t _audio_sequence_number = 0U;

	// To convert from timebase to seconds, multiply by these value
	double _video_timebase_expr = 0.0;
	double _audio_timebase_expr = 0.0;

	// To convert from timebase to milliseconds, multiply by these value
	double _video_timebase_expr_ms = 0.0;
	double _audio_timebase_expr_ms = 0.0;

	// To convert from seconds to timebase, multiply by these value
	double _video_timescale = 0.0;
	double _audio_timescale = 0.0;

	// Date & Time (YYYY-MM-DDTHH:II:SS.sssZ)
	ov::String _start_time;
	int64_t _start_time_ms = -1LL;

	ov::String _pixel_aspect_ratio;
	double _mpd_min_buffer_time;

	// Unit: Timebase of the track
	int64_t _ideal_duration_for_video = 0.0;
	int64_t _ideal_duration_for_audio = 0.0;

	// Unit: millisecond
	int64_t _ideal_duration_for_video_in_ms = 0.0;
	int64_t _ideal_duration_for_audio_in_ms = 0.0;

	std::shared_ptr<SegmentItem> _video_init_file = nullptr;
	std::shared_ptr<SegmentItem> _audio_init_file = nullptr;

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
	int64_t _duration_delta_for_video = 0.0;
	int64_t _duration_delta_for_audio = 0.0;

	// Unit: millisecond
	int64_t _video_start_time = -1LL;
	int64_t _audio_start_time = -1LL;

	// First packet PTS of the segment
	// Unit: Timebase of the track
	int64_t _first_video_pts = -1LL;
	int64_t _first_audio_pts = -1LL;
	// Last received packet PTS
	// Unit: Timebase of the track
	int64_t _last_video_pts = -1LL;
	int64_t _last_audio_pts = -1LL;

	double _duration_margin;

	Writer _video_m4s_writer;
	Writer _audio_m4s_writer;

	ov::StopWatch _stat_stop_watch;
};