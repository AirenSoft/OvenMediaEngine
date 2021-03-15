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
#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>

#include "packetizer_define.h"
#include "chunked_transfer_interface.h"

class Packetizer
{
public:
	Packetizer(const ov::String &app_name, const ov::String &stream_name,
			   uint32_t segment_count, uint32_t segment_save_count, uint32_t segment_duration,
			   const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
			   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	virtual ~Packetizer() = default;

	virtual const char *GetPacketizerName() const = 0;

	virtual bool AppendVideoFrame(const std::shared_ptr<const MediaPacket> &media_packet) = 0;
	virtual bool AppendAudioFrame(const std::shared_ptr<const MediaPacket> &media_packet) = 0;

	// Deprecated API - Only used in CmafPacketizer
	virtual bool AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &frame) = 0;
	virtual bool AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &frame) = 0;

	virtual std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const = 0;
	// virtual bool SetSegmentData(ov::String file_name, uint64_t duration_in_ms, int64_t timestamp_in_ms, const std::shared_ptr<const ov::Data> &data) = 0;

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

	void SetPlayList(const ov::String &play_list);

	virtual bool IsReadyForStreaming() const noexcept;
	virtual bool GetPlayList(ov::String &play_list);

	bool GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentItem>> &segment_datas);
	bool GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentItem>> &segment_datas);

	static int64_t GetTimestampInMs();
	static ov::String MakeUtcSecond(time_t value);
	static ov::String MakeUtcMillisecond(int64_t value = -1LL);
	static int64_t GetCurrentMilliseconds();
	static int64_t GetCurrentTick();

protected:
	virtual void SetReadyForStreaming() noexcept;

	static ov::String GetCodecString(const std::shared_ptr<const MediaTrack> &track);

	ov::String _app_name;
	ov::String _stream_name;

	uint32_t _segment_count = 0U;
	uint32_t _segment_save_count = 0U;
	// Duration in second
	double _segment_duration = 0.0;

	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;

	std::shared_ptr<ChunkedTransferInterface> _chunked_transfer;

	uint32_t _sequence_number = 1U;
	bool _streaming_start = false;

	bool _video_key_frame_received = false;
	bool _audio_key_frame_received = false;

	uint32_t _current_video_index = 0U;
	uint32_t _current_audio_index = 0U;

	ov::String _play_list;
	std::vector<std::shared_ptr<SegmentItem>> _video_segments;
	// HLS packetizer doesn't use _audio_segments
	std::vector<std::shared_ptr<SegmentItem>> _audio_segments;

	mutable std::mutex _play_list_mutex;
	mutable std::mutex _video_segment_mutex;
	mutable std::mutex _audio_segment_mutex;
};
