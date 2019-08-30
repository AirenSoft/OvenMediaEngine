//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_packetizer.h"
#include "dash_define.h"
#include "dash_private.h"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

// Unit: seconds
#define DURATION_MARGIN (0.1)

DashPacketizer::DashPacketizer(const ov::String &app_name, const ov::String &stream_name,
							   PacketizerStreamType stream_type,
							   const ov::String &segment_prefix,
							   uint32_t segment_count, uint32_t segment_duration,
							   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track)
	: Packetizer(app_name, stream_name,
				 PacketizerType::Dash,
				 stream_type,
				 segment_prefix,
				 segment_count, segment_duration,
				 video_track, audio_track)
{
	_mpd_min_buffer_time = 6;

	// Validate video_track and audio_track for debugging
	switch (stream_type)
	{
		case PacketizerStreamType::Common:
			OV_ASSERT2(video_track != nullptr);
			OV_ASSERT2(audio_track != nullptr);
			break;

		case PacketizerStreamType::VideoOnly:
			OV_ASSERT2(video_track != nullptr);
			break;

		case PacketizerStreamType::AudioOnly:
			OV_ASSERT2(audio_track != nullptr);
			break;
	}

	if (video_track != nullptr)
	{
		uint32_t resolution_gcd = Gcd(video_track->GetWidth(), video_track->GetHeight());

		if (resolution_gcd != 0)
		{
			std::ostringstream pixel_aspect_ratio;
			pixel_aspect_ratio << video_track->GetWidth() / resolution_gcd << ":"
							   << video_track->GetHeight() / resolution_gcd;

			_pixel_aspect_ratio = pixel_aspect_ratio.str();
		}

		// Since each frame cannot be divided exactly by the length of duration due to problems such as GOP, leave a margin of about 100 ms.
		_ideal_duration_for_video = (_segment_duration - DURATION_MARGIN) * _video_track->GetTimeBase().GetTimescale();
	}

	if (audio_track != nullptr)
	{
		_ideal_duration_for_audio = _segment_duration * _audio_track->GetTimeBase().GetTimescale();
	}
}

DashFileType DashPacketizer::GetFileType(const ov::String &file_name)
{
	if (file_name.IndexOf(DASH_MPD_VIDEO_INIT_FILE_NAME) >= 0)
	{
		return DashFileType::VideoInit;
	}
	else if (file_name.IndexOf(DASH_MPD_AUDIO_INIT_FILE_NAME) >= 0)
	{
		return DashFileType::AudioInit;
	}
	else if (file_name.IndexOf(DASH_MPD_VIDEO_SUFFIX) >= 0)
	{
		return DashFileType::VideoSegment;
	}
	else if (file_name.IndexOf(DASH_MPD_AUDIO_SUFFIX) >= 0)
	{
		return DashFileType::AudioSegment;
	}

	return DashFileType::Unknown;
}

int DashPacketizer::GetStartPatternSize(const uint8_t *buffer)
{
	// 0x00 0x00 0x00 0x01 pattern
	if (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 1)
	{
		return 4;
	}
	// 0x00 0x00 0x01 pattern
	else if (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 1)
	{
		return 3;
	}

	// Unknown pattern
	return 0;
}

ov::String DashPacketizer::GetFileName(int64_t start_timestamp, common::MediaType media_type) const
{
	switch (media_type)
	{
		case common::MediaType::Video:
			return ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, DASH_MPD_VIDEO_FULL_SUFFIX);

		case common::MediaType::Audio:
			return ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, DASH_MPD_AUDIO_FULL_SUFFIX);
	}

	return "";
}

bool DashPacketizer::WriteVideoInitInternal(const std::shared_ptr<ov::Data> &frame, M4sTransferType transfer_type, const ov::String &init_file_name)
{
	uint32_t current_index = 0;
	int sps_start_index = -1;
	int sps_end_index = -1;
	int pps_start_index = -1;
	int pps_end_index = -1;
	int total_start_pattern_size = 0;

	auto buffer = frame->GetDataAs<uint8_t>();

	// Parse SPS/PPS (0001/001 + SPS + 0001/001 + PPS)
	while ((current_index + AVC_NAL_START_PATTERN_SIZE) < frame->GetLength())
	{
		int start_pattern_size = GetStartPatternSize(buffer + current_index);

		if (start_pattern_size == 0)
		{
			current_index++;
			continue;
		}

		total_start_pattern_size += start_pattern_size;

		if (sps_start_index == -1)
		{
			sps_start_index = current_index + start_pattern_size;
			current_index += start_pattern_size;
			continue;
		}
		else if (sps_end_index == -1)
		{
			sps_end_index = current_index - 1;
			pps_start_index = current_index + start_pattern_size;
			current_index += start_pattern_size;
			continue;
		}
		else
		{
			pps_end_index = current_index - 1;
			break;
		}
	}

	// Check parsing result
	if ((sps_start_index == -1) || (sps_end_index < (sps_start_index + 4)))
	{
		logte("Could not parse SPS (SPS: %d-%d, PPS: %d-%d) from DASH frame for [%s/%s]",
			  sps_start_index, sps_end_index, pps_start_index, pps_end_index,
			  _app_name.CStr(), _stream_name.CStr());

		return false;
	}

	if ((pps_start_index <= sps_end_index) || (pps_start_index >= pps_end_index))
	{
		logte("Could not parse PPS (SPS: %d-%d, PPS: %d-%d) from DASH frame for [%s/%s]",
			  sps_start_index, sps_end_index, pps_start_index, pps_end_index,
			  _app_name.CStr(), _stream_name.CStr());

		return false;
	}

	if ((total_start_pattern_size < 9) || (total_start_pattern_size > 12))
	{
		logte("Invalid patterns (%d, SPS: %d-%d, PPS: %d-%d) in DASH frame for [%s/%s]",
			  total_start_pattern_size,
			  sps_start_index, sps_end_index, pps_start_index, pps_end_index,
			  _app_name.CStr(), _stream_name.CStr());

		return false;
	}

	// Extract SPS from frame
	auto avc_sps = frame->Subdata(sps_start_index, sps_end_index - sps_start_index + 1);

	// Extract PPS from frame
	auto avc_pps = frame->Subdata(pps_start_index, pps_end_index - pps_start_index + 1);

	// Create an init m4s for video stream
	M4sInitWriter writer(M4sMediaType::Video, _segment_duration * _video_track->GetTimeBase().GetTimescale(), _video_track, _audio_track, avc_sps, avc_pps);

	auto init_data = writer.CreateData(transfer_type);

	if (init_data == nullptr)
	{
		logte("Could not write DASH init file for video [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	_avc_nal_header_size = avc_sps->GetLength() + avc_pps->GetLength() + total_start_pattern_size;

	// Store data for video stream
	_video_init_file = std::make_shared<SegmentData>(common::MediaType::Video, 0, init_file_name, 0, 0, init_data);

	logtd("DASH init file (%s) is written for video [%s/%s]", init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool DashPacketizer::WriteAudioInitInternal(const std::shared_ptr<ov::Data> &frame, M4sTransferType transfer_type, const ov::String &init_file_name)
{
	M4sInitWriter writer(M4sMediaType::Audio, _segment_duration * _audio_track->GetTimeBase().GetTimescale(), _video_track, _audio_track, nullptr, nullptr);

	// Create an init m4s for audio stream
	auto init_data = writer.CreateData(transfer_type);

	if (init_data == nullptr)
	{
		logte("Could not write DASH init file for audio [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Store data for audio stream
	_audio_init_file = std::make_shared<SegmentData>(common::MediaType::Audio, 0, init_file_name, 0, 0, init_data);

	logtd("DASH init file (%s) is written for audio [%s/%s]", init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool DashPacketizer::WriteVideoInit(const std::shared_ptr<ov::Data> &frame)
{
	return WriteVideoInitInternal(frame, M4sTransferType::Normal, DASH_MPD_VIDEO_FULL_INIT_FILE_NAME);
}

bool DashPacketizer::WriteAudioInit(const std::shared_ptr<ov::Data> &frame)
{
	return WriteAudioInitInternal(frame, M4sTransferType::Normal, DASH_MPD_AUDIO_FULL_INIT_FILE_NAME);
}

bool DashPacketizer::WriteVideoInitIfNeeded(std::shared_ptr<PacketizerFrameData> &frame)
{
	if (_video_init)
	{
		return true;
	}

	if ((frame->type == PacketizerFrameType::VideoKeyFrame) && WriteVideoInit(frame->data))
	{
		_video_init = true;
	}

	return _video_init;
}

bool DashPacketizer::WriteAudioInitIfNeeded(std::shared_ptr<PacketizerFrameData> &frame)
{
	if (_audio_init)
	{
		return true;
	}

	_audio_init = WriteAudioInit(frame->data);

	return _audio_init;
}

bool DashPacketizer::AppendVideoFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, std::function<void(const std::shared_ptr<const SampleData> &data)> data_callback)
{
	if (WriteVideoInitIfNeeded(frame) == false)
	{
		return false;
	}

	auto &data = frame->data;

	// Calculate offset to skip NAL header
	int offset = (frame->type == PacketizerFrameType::VideoKeyFrame) ? _avc_nal_header_size : GetStartPatternSize(data->GetDataAs<uint8_t>());

	if (data->GetLength() < offset)
	{
		// Not enough data
		logtw("Invalid frame: frame is too short: %zu bytes", data->GetLength());
		return false;
	}

	// Skip NAL header
	data = data->Subdata(offset);

	// Check whether the incoming frame is a key frame
	if (frame->type == PacketizerFrameType::VideoKeyFrame)
	{
		// Check the timestamp to determine if a new segment is to be created
		if (current_segment_duration >= _ideal_duration_for_video)
		{
			// Need to create a new segment

			// Flush frames
			if (WriteVideoSegment() == false)
			{
				logte("An error occurred while write the DASH video segment");
				return false;
			}

			UpdatePlayList();
		}
	}
	else
	{
		// If the frame is not key frame, it cannot be the beginning of a new segment
		// So append it to the current segment
	}

	if (_start_time.IsEmpty())
	{
		_start_time = MakeUtcSecond(::time(nullptr));
	}

	// 8.8.3 Track Extends Box
	// The sample flags field in sample fragments (default_sample_flags here and in a Track Fragment Header Box,
	// and sample_flags and first_sample_flags in a Track Fragment Run Box) is coded as a 32-bit value.
	// It has the following structure:
	//
	// (R) bit(4)           reserved=0;
	// (A) unsigned int(2)  is_leading;
	// (B) unsigned int(2)  sample_depends_on;
	// (C) unsigned int(2)  sample_is_depended_on;
	// (D) unsigned int(2)  sample_has_redundancy;
	// (E) bit(3)           sample_padding_value;
	// (F) bit(1)           sample_is_non_sync_sample;
	// (G) unsigned int(16) sample_degradation_priority;
	//
	//              01234567 01234567 01234567 01234567
	//              RRRRAABB CCDDEEEF GGGGGGGG GGGGGGGG
	// 0x02000000 = 00000010 00000000 00000000 00000000 (sample_depends_on == 2)
	// 0x01010000 = 00000001 00000001 00000000 00000000 (sample_depends_on == 1, sample_is_non_sync_sample = 1)
	uint32_t flag = (frame->type == PacketizerFrameType::VideoKeyFrame) ? 0X02000000 : 0X01010000;

	if (data_callback != nullptr)
	{
		data_callback(std::make_shared<SampleData>(frame->duration, flag, frame->timestamp, frame->time_offset, frame->data));
	}

	return true;
}

bool DashPacketizer::AppendAudioFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, std::function<void(const std::shared_ptr<const SampleData> &data)> data_callback)
{
	if (WriteAudioInitIfNeeded(frame) == false)
	{
		return false;
	}

	auto &data = frame->data;

	// Skip ADTS header
	frame->data = frame->data->Subdata(ADTS_HEADER_SIZE);

	// Since audio frame is always a key frame, don't need to check the frame type

	// Check the timestamp to determine if a new segment is to be created
	if (current_segment_duration >= _ideal_duration_for_audio)
	{
		// Need to create a new segment

		// Flush frames
		if (WriteAudioSegment() == false)
		{
			logte("An error occurred while write the DASH audio segment");
			return false;
		}

		UpdatePlayList();
	}

	if (_start_time.IsEmpty())
	{
		_start_time = MakeUtcSecond(::time(nullptr));
	}

	if (data_callback != nullptr)
	{
		data_callback(std::make_shared<SampleData>(frame->duration, frame->timestamp, frame->data));
	}

	return true;
}

bool DashPacketizer::AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	int64_t current_segment_duration = (_video_datas.empty()) ? 0ULL : frame->timestamp - _video_datas.front()->timestamp;

	return AppendVideoFrameInternal(frame, current_segment_duration, [frame, this](const std::shared_ptr<const SampleData> &data) {
		_video_datas.push_back(data);
		_last_video_pts = frame->timestamp;
	});
}

bool DashPacketizer::AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	int64_t current_segment_duration = (_audio_datas.empty()) ? 0ULL : frame->timestamp - _audio_datas.front()->timestamp;

	return AppendAudioFrameInternal(frame, current_segment_duration, [frame, this](const std::shared_ptr<const SampleData> &data) {
		_audio_datas.push_back(data);
		_last_audio_pts = frame->timestamp;
	});
}

bool DashPacketizer::WriteVideoSegment()
{
	if (_video_datas.empty())
	{
		logtd("There is no video data for DASH segment");
		return true;
	}

	auto sample_datas = std::move(_video_datas);
	int64_t start_timestamp = sample_datas.front()->timestamp;
	const auto &last_frame = sample_datas.back();
	auto segment_duration = last_frame->timestamp + last_frame->duration - start_timestamp;

	// Create a fragment
	M4sSegmentWriter writer(M4sMediaType::Video, _video_sequence_number, 1, start_timestamp);

	auto segment_data = writer.AppendSamples(sample_datas);

	if (segment_data == nullptr)
	{
		logte("Could not write video samples (%zu) to SegmentWriter for [%s/%s]", sample_datas.size(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Append the data to the m4s list
	if (SetSegmentData(GetFileName(start_timestamp, common::MediaType::Video), segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_video_sequence_number++;

	return true;
}

bool DashPacketizer::WriteAudioSegment()
{
	if (_audio_datas.empty())
	{
		logtd("There is no audio data for DASH segment");
		return true;
	}

	auto sample_datas = std::move(_audio_datas);
	int64_t start_timestamp = sample_datas.front()->timestamp;

	// Create a fragment
	M4sSegmentWriter writer(M4sMediaType::Audio, _audio_sequence_number, 2, start_timestamp);
	auto segment_data = writer.AppendSamples(sample_datas);

	if (segment_data == nullptr)
	{
		logte("Could not write audio samples (%zu) to SegmentWriter for [%s/%s]", sample_datas.size(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Enqueue the segment
	const auto &last_frame = sample_datas.back();
	auto segment_duration = last_frame->timestamp + last_frame->duration - start_timestamp;

	if (SetSegmentData(GetFileName(start_timestamp, common::MediaType::Audio), segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_audio_sequence_number++;

	return true;
}

bool DashPacketizer::GetSegmentInfos(ov::String *video_urls, ov::String *audio_urls, double *time_shift_buffer_depth, double *minimum_update_period)
{
	OV_ASSERT2(video_urls != nullptr);
	OV_ASSERT2(audio_urls != nullptr);
	OV_ASSERT2(time_shift_buffer_depth != nullptr);
	OV_ASSERT2(minimum_update_period != nullptr);

	double time_shift_buffer_depth_for_video = static_cast<double>(UINT64_MAX);
	double time_shift_buffer_depth_for_audio = static_cast<double>(UINT64_MAX);
	double minimum_update_period_for_video = static_cast<double>(UINT64_MAX);
	double minimum_update_period_for_audio = static_cast<double>(UINT64_MAX);

	bool calculated = false;

	std::vector<std::shared_ptr<SegmentData>> video_segment_datas;

	if (Packetizer::GetVideoPlaySegments(video_segment_datas) == false)
	{
		return false;
	}

	if (video_segment_datas.empty() == false)
	{
		int index = 0;
		uint64_t video_total_duration = 0ULL;
		uint64_t video_last_duration = 0ULL;
		std::ostringstream urls;

		for (const auto &segment : video_segment_datas)
		{
			// urls = [				<S]
			urls << "\t\t\t\t<S";

			if (index == 0)
			{
				// urls = ...[ t="<timestamp>"]
				urls << " t=\"" << segment->timestamp << "\"";
			}

			// urls = ...[ d="<duration>"/>\n]
			urls << " d=\"" << segment->duration << "\"/>\n";

			video_total_duration += segment->duration;
			index++;
		}

		video_last_duration = video_segment_datas.back()->duration;
		*video_urls = urls.str().c_str();

		auto timescale = _video_track->GetTimeBase().GetTimescale();

		if (timescale != 0.0)
		{
			time_shift_buffer_depth_for_video = video_total_duration / timescale;
			minimum_update_period_for_video = video_last_duration / timescale;

			calculated = true;
		}
	}

	std::vector<std::shared_ptr<SegmentData>> audio_segment_datas;

	if (Packetizer::GetAudioPlaySegments(audio_segment_datas) == false)
	{
		return false;
	}

	if (audio_segment_datas.empty() == false)
	{
		int index = 0;
		uint64_t audio_total_duration = 0ULL;
		uint64_t audio_last_duration = 0ULL;
		std::ostringstream urls;

		for (const auto &segment : audio_segment_datas)
		{
			// urls = [				<S]
			urls << "\t\t\t\t<S";

			if (index == 0)
			{
				// urls = ...[ t="<timestamp>"]
				urls << " t=\"" << segment->timestamp << "\"";
			}

			// urls = ...[ d="<duration>"/>\n]
			urls << " d=\"" << segment->duration << "\"/>\n";

			audio_total_duration += segment->duration;
			index++;
		}

		audio_last_duration = audio_segment_datas.back()->duration;
		*audio_urls = urls.str().c_str();

		auto timescale = _audio_track->GetTimeBase().GetTimescale();

		if (timescale != 0.0)
		{
			time_shift_buffer_depth_for_audio = audio_total_duration / timescale;
			minimum_update_period_for_audio = audio_last_duration / timescale;

			calculated = true;
		}
	}

	if (calculated = false)
	{
		return false;
	}

	*time_shift_buffer_depth = std::min(time_shift_buffer_depth_for_video, time_shift_buffer_depth_for_audio);
	*minimum_update_period = std::min(minimum_update_period_for_video, minimum_update_period_for_audio);

	return true;
}

bool DashPacketizer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
	ov::String video_urls;
	ov::String audio_urls;
	double time_shift_buffer_depth = 0;
	double minimumUpdatePeriod = 0;

	logtd("Trying to update playlist for DASH...");

	if (GetSegmentInfos(&video_urls, &audio_urls, &time_shift_buffer_depth, &minimumUpdatePeriod) == false)
	{
		logtd("Could not obtain segment info for stream [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	play_list_stream << std::fixed << std::setprecision(3)
					 << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
						"<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
						"\txmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
						"\txmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
						"\txsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
						"\tprofiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
						"\ttype=\"dynamic\"\n"
					 << "\tminimumUpdatePeriod=\"PT" << minimumUpdatePeriod << "S\"\n"
					 << "\tpublishTime=\"" << MakeUtcSecond(::time(nullptr)).CStr() << "\"\n"
					 << "\tavailabilityStartTime=\"" << _start_time.CStr() << "\"\n"
					 << "\ttimeShiftBufferDepth=\"PT" << time_shift_buffer_depth << "S\"\n"
					 << "\tsuggestedPresentationDelay=\"PT" << std::setprecision(1) << _segment_duration << "S\"\n"
					 << "\tminBufferTime=\"PT" << _mpd_min_buffer_time << "S\">\n"
					 << "\t<Period id=\"0\" start=\"PT0S\">\n";

	if (video_urls.IsEmpty() == false)
	{
		play_list_stream << "\t\t<AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" "
						 << "width=\"" << _video_track->GetWidth() << "\" height=\"" << _video_track->GetHeight()
						 << "\" par=\"" << _pixel_aspect_ratio << "\" frameRate=\"" << _video_track->GetFrameRate()
						 << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
						 << "\t\t\t<SegmentTemplate timescale=\"" << static_cast<uint32_t>(_video_track->GetTimeBase().GetTimescale())
						 << "\" initialization=\"" << DASH_MPD_VIDEO_FULL_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << DASH_MPD_VIDEO_FULL_SUFFIX << "\">\n"
						 << "\t\t\t\t<SegmentTimeline>\n"
						 << video_urls.CStr()
						 << "\t\t\t\t</SegmentTimeline>\n"
						 << "\t\t\t</SegmentTemplate>\n"
						 << "\t\t\t<Representation codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _video_track->GetBitrate()
						 << "\" />\n"
						 << "\t\t</AdaptationSet>\n";
	}

	if (audio_urls.IsEmpty() == false)
	{
		play_list_stream << "\t\t<AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
						 << "\t\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\""
						 << _audio_track->GetChannel().GetCounts() << "\"/>\n"
						 << "\t\t\t<SegmentTemplate timescale=\"" << static_cast<uint32_t>(_audio_track->GetTimeBase().GetTimescale())
						 << "\" initialization=\"" << DASH_MPD_AUDIO_FULL_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << DASH_MPD_AUDIO_FULL_SUFFIX << "\">\n"
						 << "\t\t\t\t<SegmentTimeline>\n"
						 << audio_urls.CStr()
						 << "\t\t\t\t</SegmentTimeline>\n"
						 << "\t\t\t</SegmentTemplate>\n"
						 << "\t\t\t<Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _audio_track->GetSampleRate()
						 << "\" bandwidth=\"" << _audio_track->GetBitrate() << "\" />\n"
						 << "\t\t</AdaptationSet>\n";
	}

	play_list_stream << "\t</Period>\n"
					 << "\t<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=\"%s\"/>\n"
					 << "</MPD>\n";

	ov::String play_list = play_list_stream.str().c_str();

	SetPlayList(play_list);

	if ((_last_video_pts >= 0LL) && (_last_audio_pts >= 0LL))
	{
		int64_t video_pts = static_cast<int64_t>(_last_video_pts * _video_track->GetTimeBase().GetExpr() * 1000.0);
		int64_t audio_pts = static_cast<int64_t>(_last_audio_pts * _audio_track->GetTimeBase().GetExpr() * 1000.0);

		logtd("Time difference: A-V: %lld (Audio: %lld, Video: %lld)", audio_pts - video_pts, audio_pts, video_pts);
	}

	return true;
}

const std::shared_ptr<SegmentData> DashPacketizer::GetSegmentData(const ov::String &file_name)
{
	if (_streaming_start == false)
	{
		logtd("Could not obtain segment data for %s, stream is not started", file_name.CStr());

		return nullptr;
	}

	auto file_type = GetFileType(file_name);

	switch (file_type)
	{
		case DashFileType::VideoSegment:
		{
			std::unique_lock<std::mutex> lock(_video_segment_guard);

			auto item = std::find_if(_video_segment_datas.begin(), _video_segment_datas.end(),
									 [&](std::shared_ptr<SegmentData> const &value) -> bool {
										 return value != nullptr ? value->file_name == file_name : false;
									 });

			return (item != _video_segment_datas.end()) ? (*item) : nullptr;
		}

		case DashFileType::AudioSegment:
		{
			std::unique_lock<std::mutex> lock(_audio_segment_guard);

			auto item = std::find_if(_audio_segment_datas.begin(), _audio_segment_datas.end(),
									 [&](std::shared_ptr<SegmentData> const &value) -> bool {
										 return value != nullptr ? value->file_name == file_name : false;
									 });

			return (item != _audio_segment_datas.end()) ? (*item) : nullptr;
		}

		case DashFileType::VideoInit:
			return _video_init_file;

		case DashFileType::AudioInit:
			return _audio_init_file;

		default:
			break;
	}

	logtd("Unknown type is requested: %s", file_name.CStr());

	return nullptr;
}

bool DashPacketizer::SetSegmentData(ov::String file_name, uint64_t duration, int64_t timestamp, std::shared_ptr<ov::Data> &data)
{
	auto file_type = GetFileType(file_name);

	switch (file_type)
	{
		case DashFileType::VideoSegment:
		{
			// video segment mutex
			std::unique_lock<std::mutex> lock(_video_segment_guard);

			_video_segment_datas[_current_video_index++] = std::make_shared<SegmentData>(common::MediaType::Video, _sequence_number++, file_name, timestamp, duration, data);

			if (_segment_save_count <= _current_video_index)
			{
				_current_video_index = 0;
			}

			_video_sequence_number++;

			logtd("DASH segment is added for video stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, data->GetLength(), duration, _video_track->GetTimeBase().GetTimescale(), (double)duration / _video_track->GetTimeBase().GetTimescale());

			break;
		}

		case DashFileType::AudioSegment:
		{
			// audio segment mutex
			std::unique_lock<std::mutex> lock(_audio_segment_guard);

			_audio_segment_datas[_current_audio_index++] = std::make_shared<SegmentData>(common::MediaType::Audio, _sequence_number++, file_name, timestamp, duration, data);

			if (_segment_save_count <= _current_audio_index)
			{
				_current_audio_index = 0;
			}

			_audio_sequence_number++;

			logtd("DASH segment is added for audio stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, data->GetLength(), duration, _audio_track->GetTimeBase().GetTimescale(), (double)duration / _audio_track->GetTimeBase().GetTimescale());

			break;
		}
	}

	if ((_streaming_start == false) && (((_video_track == nullptr) || (_video_sequence_number > _segment_count)) &&
										((_audio_track == nullptr) || (_audio_sequence_number > _segment_count))))
	{
		_streaming_start = true;

		logti("DASH segment is ready for stream [%s/%s], segment duration: %fs, count: %u", _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

	return true;
}

bool DashPacketizer::GetPlayList(ov::String &play_list)
{
	if (_streaming_start == false)
	{
		logtd("A playlist was requested before the stream began");
		return false;
	}

	ov::String current_time = MakeUtcMillisecond();

	play_list = ov::String::FormatString(_play_list.CStr(), current_time.CStr());

	return true;
}
