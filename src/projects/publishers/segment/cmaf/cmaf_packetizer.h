//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <publishers/segment/segment_stream/packetizer/cmaf_chunk_writer.h>

#include "../dash/dash_packetizer.h"

class CmafPacketizer : public Packetizer
{
public:
	CmafPacketizer(const ov::String &app_name, const ov::String &stream_name,
				   uint32_t segment_count, uint32_t segment_duration,
				   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
				   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	virtual const char *GetPacketizerName() const
	{
		return "LLDASH";
	}

	static DashFileType GetFileType(const ov::String &file_name);
	static int GetStartPatternSize(const uint8_t *buffer, const size_t buffer_len);
	ov::String GetFileName(cmn::MediaType media_type) const;

	bool WriteVideoInit(const std::shared_ptr<const ov::Data> &frame);
	bool WriteAudioInit(const std::shared_ptr<const ov::Data> &frame);

	bool WriteVideoSegment();
	bool WriteAudioSegment();

	bool WriteVideoInitIfNeeded(const std::shared_ptr<const PacketizerFrameData> &frame);
	bool WriteAudioInitIfNeeded(const std::shared_ptr<const PacketizerFrameData> &frame);

	//--------------------------------------------------------------------
	// Override Packetizer
	//--------------------------------------------------------------------
	bool AppendVideoFrame(const std::shared_ptr<const MediaPacket> &media_packet) override
	{
		return false;
	}
	bool AppendAudioFrame(const std::shared_ptr<const MediaPacket> &media_packet) override
	{
		return false;
	}
	bool AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &frame) override;
	bool AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &frame) override;

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const override;
	bool SetSegmentData(ov::String file_name, int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms, const std::shared_ptr<const ov::Data> &data);

	bool GetPlayList(ov::String &play_list) override;

protected:
	using DataCallback = std::function<void(const std::shared_ptr<const SampleData> &data, bool new_segment_written)>;

	bool WriteVideoInitInternal(const std::shared_ptr<const ov::Data> &frame, const ov::String &init_file_name);
	// Enqueues the video frame, and call the data_callback if a new segment is created
	bool AppendVideoFrameInternal(const std::shared_ptr<const PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback);

	bool WriteAudioInitInternal(const std::shared_ptr<const ov::Data> &frame, const ov::String &init_file_name);
	// Enqueues the audio frame, and call the data_callback if a new segment is created
	bool AppendAudioFrameInternal(const std::shared_ptr<const PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback);

	void SetReadyForStreaming() noexcept override;

	ov::String MakeJitterStatString(int64_t elapsed_time, int64_t current_time, int64_t jitter, int64_t adjusted_jitter, int64_t new_jitter_correction, int64_t video_delta, int64_t audio_delta, int64_t stream_delta) const;
	void DoJitterCorrection();
	bool UpdatePlayList();

private:
	bool _video_enable = false;
	bool _audio_enable = false;

	int _avc_nal_header_size = 0;
	// Date & Time (YYYY-MM-DDTHH:II:SS.sssZ)
	ov::String _start_time;
	int64_t _start_time_ms = -1LL;
	ov::String _pixel_aspect_ratio;
	double _mpd_min_buffer_time;

	double _ideal_duration_for_video = 0.0;
	double _ideal_duration_for_audio = 0.0;

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
	double _duration_delta_for_video = 0.0;
	double _duration_delta_for_audio = 0.0;

	uint32_t _video_segment_count = 0U;
	uint32_t _audio_segment_count = 0U;

	// Unit: milliseconds
	int64_t _video_start_time = -1LL;
	int64_t _audio_start_time = -1LL;

	std::vector<std::shared_ptr<const SampleData>> _video_datas;
	int64_t _first_video_pts = -1LL;
	int64_t _last_video_pts = -1LL;
	double _video_scale = 0.0;
	std::vector<std::shared_ptr<const SampleData>> _audio_datas;
	int64_t _first_audio_pts = -1LL;
	int64_t _last_audio_pts = -1LL;
	double _audio_scale = 0.0;

	double _duration_margin;

	ov::StopWatch _stat_stop_watch;

	std::shared_ptr<CmafChunkWriter> _video_chunk_writer = nullptr;
	std::shared_ptr<CmafChunkWriter> _audio_chunk_writer = nullptr;

	bool _is_first_video_frame = true;
	bool _is_first_audio_frame = true;

	int64_t _jitter_correction = 0LL;
};