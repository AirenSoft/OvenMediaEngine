//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

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
	DashPacketizer(const ov::String &app_name, const ov::String &stream_name,
				   PacketizerStreamType stream_type,
				   const ov::String &segment_prefix,
				   uint32_t segment_count, uint32_t segment_duration,
				   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track);

	virtual const char *GetPacketizerName() const
	{
		return "DASH";
	}

	static DashFileType GetFileType(const ov::String &file_name);

	//--------------------------------------------------------------------
	// Override Packetizer
	//--------------------------------------------------------------------
	bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame) override;
	bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame) override;

	const std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;
	bool SetSegmentData(ov::String file_name, uint64_t duration, int64_t timestamp, std::shared_ptr<ov::Data> &data) override;

	bool GetPlayList(ov::String &play_list) override;

protected:
	using DataCallback = std::function<void(const std::shared_ptr<const SampleData> &data, bool new_segment_written)>;

	static int GetStartPatternSize(const uint8_t *buffer, const size_t buffer_len);

	// start_timestamp: a timestamp of the first frame of the segment
	virtual ov::String GetFileName(int64_t start_timestamp, cmn::MediaType media_type) const;

	bool WriteVideoInitInternal(const std::shared_ptr<ov::Data> &frame, const ov::String &init_file_name);
	virtual bool WriteVideoInit(const std::shared_ptr<ov::Data> &frame);
	virtual bool WriteVideoSegment();
	bool WriteVideoInitIfNeeded(std::shared_ptr<PacketizerFrameData> &frame);
	// Enqueues the video frame, and call the data_callback if a new segment is created
	bool AppendVideoFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback);

	bool WriteAudioInitInternal(const std::shared_ptr<ov::Data> &frame, const ov::String &init_file_name);
	virtual bool WriteAudioInit(const std::shared_ptr<ov::Data> &frame);
	virtual bool WriteAudioSegment();
	bool WriteAudioInitIfNeeded(std::shared_ptr<PacketizerFrameData> &frame);
	// Enqueues the audio frame, and call the data_callback if a new segment is created
	bool AppendAudioFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback);

	bool GetSegmentInfos(ov::String *video_urls, ov::String *audio_urls, double *time_shift_buffer_depth, double *minimum_update_period);

	virtual bool UpdatePlayList();

	virtual void SetReadyForStreaming() noexcept override;

	int _avc_nal_header_size = 0;
	// Date & Time (YYYY-MM-DDTHH:II:SS.sssZ)
	ov::String _start_time;
	int64_t _start_time_ms = -1LL;
	std::string _pixel_aspect_ratio;
	double _mpd_min_buffer_time;

	double _ideal_duration_for_video = 0.0;
	double _ideal_duration_for_audio = 0.0;

	std::shared_ptr<SegmentData> _video_init_file = nullptr;
	std::shared_ptr<SegmentData> _audio_init_file = nullptr;

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
};