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

		_ideal_duration_for_video = _segment_duration * _video_track->GetTimeBase().GetTimescale();

		if (_ideal_duration_for_video == 0.0)
		{
			_ideal_duration_for_video = 5.0;
		}

		_video_scale = _video_track->GetTimeBase().GetExpr() * 1000.0;
	}

	if (audio_track != nullptr)
	{
		_ideal_duration_for_audio = _segment_duration * _audio_track->GetTimeBase().GetTimescale();

		if (_ideal_duration_for_audio == 0.0)
		{
			_ideal_duration_for_audio = 5.0;
		}

		_audio_scale = _audio_track->GetTimeBase().GetExpr() * 1000.0;
	}

	_stat_stop_watch.Start();
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

int DashPacketizer::GetStartPatternSize(const uint8_t *buffer, const size_t buffer_len)
{
	// 0x00 0x00 0x00 0x01 pattern
	if (buffer_len >= 4 && buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 1)
	{
		return 4;
	}
	// 0x00 0x00 0x01 pattern
	else if (buffer_len >= 3 && buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 1)
	{
		return 3;
	}

	// Unknown pattern
	return 0;
}

ov::String DashPacketizer::GetFileName(int64_t start_timestamp, cmn::MediaType media_type) const
{
	switch (media_type)
	{
		case cmn::MediaType::Video:
			return ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, DASH_MPD_VIDEO_FULL_SUFFIX);

		case cmn::MediaType::Audio:
			return ov::String::FormatString("%s_%lld%s", _segment_prefix.CStr(), start_timestamp, DASH_MPD_AUDIO_FULL_SUFFIX);

		default:
			break;
	}

	return "";
}

bool DashPacketizer::WriteVideoInitInternal(const std::shared_ptr<ov::Data> &frame, const ov::String &init_file_name)
{
	const uint8_t* srcData = frame->GetDataAs<uint8_t>();
	size_t dataOffset = 0;
	size_t dataSize = frame->GetLength();

	std::vector<std::pair<size_t, size_t>> offset_list;

	// int total_start_pattern_size = 0;
	int nal_packet_header_length = 3;

	// Stage 1 - Extract the Offset and Lengh value of the NAL Packet

	while ( dataOffset < dataSize )
	{
		size_t remainDataSize = dataSize - dataOffset;
		const uint8_t* data = srcData + dataOffset;

		if (remainDataSize >= 3 && 0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2])
		{
			nal_packet_header_length = 3;
			offset_list.emplace_back(dataOffset, 3); // Offset, SIZEOF(START_CODE[3])
			dataOffset += 3;
		}
		else if (remainDataSize >= 4 && 0x00 == data[0] &&  0x00 == data[1] && 0x00 == data[2] && 0x01 == data[3])
		{
			nal_packet_header_length = 4;
			offset_list.emplace_back(dataOffset, 4); // Offset, SIZEOF(START_CODE[4])
			dataOffset += 4;
		}
		else
		{
			dataOffset += 1;
		}
	}


	// Stage 2  : Get position for SPS and PPS type

	int sps_start_index = -1;
	int sps_length = -1;
	int pps_start_index = -1;
	int pps_length = -1;

	for (size_t index = 0; index < offset_list.size(); ++index)
	{
		size_t nalu_offset = 0;
		size_t nalu_data_len = 0;

		if (index != offset_list.size() - 1)
		{
			nalu_offset = offset_list[index].first + offset_list[index].second;
			nalu_data_len = offset_list[index + 1].first - nalu_offset ;
		}
		else
		{
			nalu_offset = offset_list[index].first + offset_list[index].second;
			nalu_data_len = dataSize - nalu_offset;
		}


		// [Difinition of NAL_UNIT_TYPE]

		// - Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
		// NonIDR = 1,
		// - Coded slice data partition A slice_data_partition_a_layer_rbsp( )
		// DataPartitionA = 2,
		// - Coded slice data partition B slice_data_partition_b_layer_rbsp( )
		// DataPartitionB = 3,
		// - Coded slice data partition C slice_data_partition_c_layer_rbsp( )
		// DataPartitionC = 4,
		// - Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
		// IDR = 5,
		// - Supplemental enhancement information (SEI) sei_rbsp( )
		// SEI = 6,
		// - Sequence parameter set seq_parameter_set_rbsp( )
		// SPS = 7,
		// - Picture parameter set pic_parameter_set_rbsp( )
		// PPS = 8,
		// ...

		uint8_t nalu_header = *(srcData + nalu_offset);
		uint8_t nal_unit_type = (nalu_header)  & 0x01F;

#if 0	// for debug
		uint8_t forbidden_zero_bit = (nalu_header >> 7)  & 0x01;
		uint8_t nal_ref_idc = (nalu_header >> 5)  & 0x03;
		if ( (nal_unit_type == (uint8_t)5) || (nal_unit_type == (uint8_t)6) || (nal_unit_type == (uint8_t)7) || (nal_unit_type == (uint8_t)8))
		{
			logte("[%d] nal_ref_idc:%2d, nal_unit_type:%2d => offset:%d, nalu_size:%d, nalu_offset:%d, nalu_length:%d"
				, index
				, nal_ref_idc, nal_unit_type
				, offset_list[index].first
				, offset_list[index].second
				, nalu_offset
				, nalu_data_len);
		}
#endif

		// SPS type
		if (nal_unit_type == 7)
		{
			sps_start_index = nalu_offset;
			sps_length = nalu_data_len;
		}
		// PPS type
		else if (nal_unit_type == 8)
		{
			pps_start_index = nalu_offset;
			pps_length = nalu_data_len;
		}
	}

	// logte("nal_packet_header_length : %d", nal_packet_header_length);

	// Check parsing result
	if ((sps_start_index == -1) || (sps_length < -1))
	{
		logte("Could not parse SPS (SPS: %d-%d, PPS: %d-%d) from %s frame for [%s/%s]",
			sps_start_index, sps_length, pps_start_index, pps_length,
			GetPacketizerName(),
			_app_name.CStr(), _stream_name.CStr());

		return false;
	}

	if ((pps_start_index == -1) || (pps_length < -1))
	{
		logte("Could not parse PPS (SPS: %d-%d, PPS: %d-%d) from %s frame for [%s/%s]",
			sps_start_index, sps_length, pps_start_index, pps_length,
			GetPacketizerName(),
			_app_name.CStr(), _stream_name.CStr());

		return false;
	}

	// Notice: One packet may contain multiple NAL packets. so, Removed the maximum pattern size limit.
	// if ((total_start_pattern_size < 6) || (total_start_pattern_size > 12))

	if ( !(nal_packet_header_length == 3 || nal_packet_header_length == 4) )
	{
		logte("Invalid patterns (start code length : %d, SPS: %d-%d, PPS: %d-%d) in %s frame for [%s/%s]",
			nal_packet_header_length,
			sps_start_index, sps_length, pps_start_index, pps_length,
			GetPacketizerName(),
			_app_name.CStr(), _stream_name.CStr());

		return false;
	}


	//Stage 3 : Extracts SPS/PPS data to create initialization packets.

	// Extract SPS from frame
	auto avc_sps = frame->Subdata(sps_start_index, sps_length);

	// Extract PPS from frame
	auto avc_pps = frame->Subdata(pps_start_index, pps_length);

	// Create an init m4s for video stream
	// init.m4s not have duration
	M4sInitWriter writer(M4sMediaType::Video, 0, _video_track, _audio_track, avc_sps, avc_pps);

	auto init_data = writer.CreateData();

	if (init_data == nullptr)
	{
		logte("Could not write %s init file for video [%s/%s]", GetPacketizerName(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// logtd("sps_lengh : %d, pps_length : %d", avc_sps->GetLength(), avc_pps->GetLength());
	_avc_nal_header_size = (nal_packet_header_length + avc_sps->GetLength()) + (nal_packet_header_length + avc_pps->GetLength());

	// Store data for video stream
	_video_init_file = std::make_shared<SegmentData>(cmn::MediaType::Video, 0, init_file_name, 0, 0, init_data);

	logtd("%s init file (%s) is written for video [%s/%s]", GetPacketizerName(), init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool DashPacketizer::WriteVideoInit(const std::shared_ptr<ov::Data> &frame)
{
	return WriteVideoInitInternal(frame, DASH_MPD_VIDEO_FULL_INIT_FILE_NAME);
}

bool DashPacketizer::WriteVideoSegment()
{
	if (_video_datas.empty())
	{
		logtd("There is no video data for %s segment", GetPacketizerName());
		return true;
	}

	auto sample_datas = std::move(_video_datas);
	// Timestamp of the first frame
	auto start_timestamp = sample_datas.front()->pts;

	// Create a fragment
	M4sSegmentWriter writer(M4sMediaType::Video, _video_segment_count, 1, start_timestamp);

	auto segment_data = writer.AppendSamples(sample_datas);

	if (segment_data == nullptr)
	{
		logte("Could not write video samples (%zu) to SegmentWriter for [%s/%s]", sample_datas.size(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Enqueue the segment
	const auto &last_frame = sample_datas.back();
	auto segment_duration = last_frame->pts + last_frame->duration - start_timestamp;

	// Append the data to the m4s list
	if (SetSegmentData(GetFileName(start_timestamp, cmn::MediaType::Video), segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_video += (_ideal_duration_for_video - segment_duration);

	_video_segment_count++;

	return true;
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

bool DashPacketizer::AppendVideoFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback)
{
	if (WriteVideoInitIfNeeded(frame) == false)
	{
		return false;
	}

	auto &data = frame->data;

	// Calculate offset to skip NAL header
	int offset = (frame->type == PacketizerFrameType::VideoKeyFrame) ? _avc_nal_header_size : GetStartPatternSize(data->GetDataAs<uint8_t>(), data->GetLength());

	if (static_cast<int>(data->GetLength()) < offset)
	{
		// Not enough data
		logtw("Invalid frame: frame is too short: expected: %d, but %zu bytes", offset, data->GetLength());
		return false;
	}

	if(frame->type == PacketizerFrameType::VideoKeyFrame)
	{
		offset += GetStartPatternSize(data->GetDataAs<uint8_t>() + offset, data->GetLength() - offset);
	}

	// Skip NAL header
	data = data->Subdata(offset);

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
	auto sample_data = std::make_shared<SampleData>(frame->duration, flag, frame->pts, frame->dts, frame->data);

	bool new_segment_written = false;

	// Check whether the incoming frame is a key frame
	if (frame->type == PacketizerFrameType::VideoKeyFrame)
	{
		if (_video_start_time == -1LL)
		{
			_video_start_time = GetCurrentMilliseconds() - current_segment_duration;
		}

		// Check the timestamp to determine if a new segment is to be created
		if ((current_segment_duration >= (_ideal_duration_for_video + _duration_delta_for_video)))
		{
			// Need to create a new segment

			// Flush frames
			if (WriteVideoSegment() == false)
			{
				logte("An error occurred while write the DASH video segment");
				return false;
			}

			new_segment_written = true;

			UpdatePlayList();
		}
	}
	else
	{
		// If the frame is not key frame, it cannot be the beginning of a new segment
		// So append it to the current segment
	}

	if (_video_start_time >= 0LL)
	{
		if (data_callback != nullptr)
		{
			data_callback(sample_data, new_segment_written);
		}
	}

	return true;
}

bool DashPacketizer::AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	int64_t start_timestamp = (_video_datas.empty()) ? frame->pts : _video_datas.front()->pts;
	int64_t current_segment_duration = frame->pts - start_timestamp;

	return AppendVideoFrameInternal(frame, current_segment_duration, [frame, this](const std::shared_ptr<const SampleData> &data, bool new_segment_written) {
		_video_datas.push_back(data);
		_last_video_pts = frame->pts;

		if(_first_video_pts == -1LL)
		{
			_first_video_pts = _last_video_pts;
		}
	});
}

bool DashPacketizer::WriteAudioInitInternal(const std::shared_ptr<ov::Data> &frame, const ov::String &init_file_name)
{
	// init.m4s does not have duration
	M4sInitWriter writer(M4sMediaType::Audio, 0, _video_track, _audio_track, nullptr, nullptr);

	// Create an init m4s for audio stream
	auto init_data = writer.CreateData();

	if (init_data == nullptr)
	{
		logte("Could not write %s init file for audio [%s/%s]", GetPacketizerName(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Store data for audio stream
	_audio_init_file = std::make_shared<SegmentData>(cmn::MediaType::Audio, 0, init_file_name, 0, 0, init_data);

	logtd("%s init file (%s) is written for audio [%s/%s]", GetPacketizerName(), init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool DashPacketizer::WriteAudioInit(const std::shared_ptr<ov::Data> &frame)
{
	return WriteAudioInitInternal(frame, DASH_MPD_AUDIO_FULL_INIT_FILE_NAME);
}

bool DashPacketizer::WriteAudioSegment()
{
	if (_audio_datas.empty())
	{
		logtd("There is no audio data for %s segment", GetPacketizerName());
		return true;
	}

	auto sample_datas = std::move(_audio_datas);
	// Timestamp of the first frame
	auto start_timestamp = sample_datas.front()->pts;

	// Create a fragment
	M4sSegmentWriter writer(M4sMediaType::Audio, _audio_segment_count, 2, start_timestamp);
	auto segment_data = writer.AppendSamples(sample_datas);

	if (segment_data == nullptr)
	{
		logte("Could not write audio samples (%zu) to SegmentWriter for [%s/%s]", sample_datas.size(), _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	// Enqueue the segment
	const auto &last_frame = sample_datas.back();
	auto segment_duration = last_frame->pts + last_frame->duration - start_timestamp;

	if (SetSegmentData(GetFileName(start_timestamp, cmn::MediaType::Audio), segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_audio += (_ideal_duration_for_audio - segment_duration);

	_audio_segment_count++;

	return true;
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

bool DashPacketizer::AppendAudioFrameInternal(std::shared_ptr<PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback)
{
	if (WriteAudioInitIfNeeded(frame) == false)
	{
		return false;
	}

	if (_audio_start_time == -1LL)
	{
		_audio_start_time = GetCurrentMilliseconds() - current_segment_duration;
	}

	// Skip ADTS header
	frame->data = frame->data->Subdata(ADTS_HEADER_SIZE);

	bool new_segment_written = false;

	// Since audio frame is always a key frame, don't need to check the frame type

	// Check the timestamp to determine if a new segment is to be created
	if ((current_segment_duration >= (_ideal_duration_for_audio + _duration_delta_for_audio)))
	{
		// Need to create a new segment

		// Flush frames
		if (WriteAudioSegment() == false)
		{
			logte("An error occurred while write the %s audio segment", GetPacketizerName());
			return false;
		}

		new_segment_written = true;

		UpdatePlayList();
	}

	if (_audio_start_time >= 0LL)
	{
		if (data_callback != nullptr)
		{
			data_callback(std::make_shared<SampleData>(frame->duration, frame->pts, frame->dts, frame->data), new_segment_written);
		}
	}

	return true;
}

bool DashPacketizer::AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	int64_t start_timestamp = (_audio_datas.empty()) ? frame->pts : _audio_datas.front()->pts;
	int64_t current_segment_duration = frame->pts - start_timestamp;

	return AppendAudioFrameInternal(frame, current_segment_duration, [frame, this](const std::shared_ptr<const SampleData> &data, bool new_segment_written) {
		_audio_datas.push_back(data);
		_last_audio_pts = frame->pts;

		if (_first_audio_pts == -1LL)
		{
			_first_audio_pts = _last_audio_pts;
		}
	});
}

bool DashPacketizer::GetSegmentInfos(ov::String *video_urls, ov::String *audio_urls, double *time_shift_buffer_depth, double *minimum_update_period)
{
	OV_ASSERT2(video_urls != nullptr);
	OV_ASSERT2(audio_urls != nullptr);
	OV_ASSERT2(time_shift_buffer_depth != nullptr);
	OV_ASSERT2(minimum_update_period != nullptr);

	double time_shift_buffer_depth_for_video = static_cast<double>(INT64_MIN);
	double time_shift_buffer_depth_for_audio = static_cast<double>(INT64_MIN);
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

	if (calculated == false)
	{
		return false;
	}

	*time_shift_buffer_depth = std::max(time_shift_buffer_depth_for_video, time_shift_buffer_depth_for_audio);
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

	if (IsReadyForStreaming() == false)
	{
		return false;
	}

	ov::String publishTime = MakeUtcSecond(::time(nullptr));

	logtd("Trying to update playlist for %s with availabilityStartTime: %s, publishTime: %s", GetPacketizerName(), _start_time.CStr(), publishTime.CStr());

	if (GetSegmentInfos(&video_urls, &audio_urls, &time_shift_buffer_depth, &minimumUpdatePeriod) == false)
	{
		logtd("Could not obtain segment info for stream [%s/%s]", _app_name.CStr(), _stream_name.CStr());
		return false;
	}

	play_list_stream
		<< std::fixed << std::setprecision(3)
		<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		   "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		   "\txmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		   "\txmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
		   "\txsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
		   "\tprofiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
		   "\ttype=\"dynamic\"\n"
		<< "\tminimumUpdatePeriod=\"PT" << minimumUpdatePeriod << "S\"\n"
		<< "\tpublishTime=\"" << publishTime.CStr() << "\"\n"
		<< "\tavailabilityStartTime=\"" << _start_time.CStr() << "\"\n"
		<< "\ttimeShiftBufferDepth=\"PT" << time_shift_buffer_depth << "S\"\n"
		<< "\tsuggestedPresentationDelay=\"PT" << std::setprecision(1) << (_segment_duration * _segment_count) << "S\"\n"
		<< "\tminBufferTime=\"PT2S\">\n"  // << _mpd_min_buffer_time << "S\">\n"
		<< "\t<Period id=\"0\" start=\"PT0S\">\n";

	if (video_urls.IsEmpty() == false)
	{
		play_list_stream
			<< "\t\t<AdaptationSet group=\"1\" mimeType=\"video/mp4\" "
			<< "width=\"" << _video_track->GetWidth() << "\" height=\"" << _video_track->GetHeight()
			<< "\" par=\"" << _pixel_aspect_ratio << "\" frameRate=\"" << _video_track->GetFrameRate()
			<< "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
			<< "\t\t\t<SegmentTemplate timescale=\"" << static_cast<uint32_t>(_video_track->GetTimeBase().GetTimescale())
			<< "\" initialization=\"" << DASH_MPD_VIDEO_FULL_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << DASH_MPD_VIDEO_FULL_SUFFIX << "\">\n"
			<< "\t\t\t\t<SegmentTimeline>\n"
			<< video_urls.CStr()
			<< "\t\t\t\t</SegmentTimeline>\n"
			<< "\t\t\t</SegmentTemplate>\n"
			<< "\t\t\t<Representation id=\"0\" codecs=\"avc1.42401f\" sar=\"1:1\" bandwidth=\"" << _video_track->GetBitrate()
			<< "\" />\n"
			<< "\t\t</AdaptationSet>\n";
	}

	if (audio_urls.IsEmpty() == false)
	{
		play_list_stream
			<< "\t\t<AdaptationSet group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
			<< "\t\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\""
			<< _audio_track->GetChannel().GetCounts() << "\"/>\n"
			<< "\t\t\t<SegmentTemplate timescale=\"" << static_cast<uint32_t>(_audio_track->GetTimeBase().GetTimescale())
			<< "\" initialization=\"" << DASH_MPD_AUDIO_FULL_INIT_FILE_NAME << "\" media=\"" << _segment_prefix.CStr() << "_$Time$" << DASH_MPD_AUDIO_FULL_SUFFIX << "\">\n"
			<< "\t\t\t\t<SegmentTimeline>\n"
			<< audio_urls.CStr()
			<< "\t\t\t\t</SegmentTimeline>\n"
			<< "\t\t\t</SegmentTemplate>\n"
			<< "\t\t\t<Representation id=\"1\" codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _audio_track->GetSampleRate()
			<< "\" bandwidth=\"" << _audio_track->GetBitrate() << "\" />\n"
			<< "\t\t</AdaptationSet>\n";
	}

	play_list_stream << "\t</Period>\n"
					 << "\t<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=\"%s\"/>\n"
					 << "</MPD>\n";

	ov::String play_list = play_list_stream.str().c_str();

	SetPlayList(play_list);

	if(_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		if ((_last_video_pts >= 0LL) && (_last_audio_pts >= 0LL))
		{
			int64_t video_pts = static_cast<int64_t>(_last_video_pts * _video_scale);
			int64_t audio_pts = static_cast<int64_t>(_last_audio_pts * _audio_scale);

			logts("[%s/%s] DASH A-V Sync: %lld (A: %lld, V: %lld)",
				_app_name.CStr(), _stream_name.CStr(),
				audio_pts - video_pts, audio_pts, video_pts);
		}
	}

	return true;
}

const std::shared_ptr<SegmentData> DashPacketizer::GetSegmentData(const ov::String &file_name)
{
	if (IsReadyForStreaming() == false)
	{
		logtd("[%p] Could not obtain segment data for %s, stream is not started", this, file_name.CStr());

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

			_video_segment_datas[_current_video_index++] = std::make_shared<SegmentData>(cmn::MediaType::Video, _sequence_number++, file_name, timestamp, duration, data);

			if (_segment_save_count <= _current_video_index)
			{
				_current_video_index = 0;
			}

			_video_segment_count++;

			logtd("%s segment is added for video stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  GetPacketizerName(), _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, data->GetLength(), duration, _video_track->GetTimeBase().GetTimescale(), (double)duration / _video_track->GetTimeBase().GetTimescale());

			break;
		}

		case DashFileType::AudioSegment:
		{
			// audio segment mutex
			std::unique_lock<std::mutex> lock(_audio_segment_guard);

			_audio_segment_datas[_current_audio_index++] = std::make_shared<SegmentData>(cmn::MediaType::Audio, _sequence_number++, file_name, timestamp, duration, data);

			if (_segment_save_count <= _current_audio_index)
			{
				_current_audio_index = 0;
			}

			_audio_segment_count++;

			logtd("%s segment is added for audio stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  GetPacketizerName(), _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration, data->GetLength(), duration, _audio_track->GetTimeBase().GetTimescale(), (double)duration / _audio_track->GetTimeBase().GetTimescale());

			break;
		}

		default:
			break;
	}

	if ((IsReadyForStreaming() == false) && (((_video_track == nullptr) || (_video_segment_count >= _segment_count)) &&
											 ((_audio_track == nullptr) || (_audio_segment_count >= _segment_count))))
	{
		SetReadyForStreaming();

		logti("[%p] %s segment is ready to stream [%s/%s], segment duration: %fs, count: %u", this, GetPacketizerName(), _app_name.CStr(), _stream_name.CStr(), _segment_duration, _segment_count);
	}

	return true;
}

void DashPacketizer::SetReadyForStreaming() noexcept
{
	_start_time_ms = std::max(_video_start_time, _audio_start_time);
	_start_time = MakeUtcMillisecond(_start_time_ms);

	Packetizer::SetReadyForStreaming();
}

bool DashPacketizer::GetPlayList(ov::String &play_list)
{
	if (IsReadyForStreaming() == false)
	{
		logtd("A playlist was requested before the stream began");
		return false;
	}

	ov::String current_time = MakeUtcMillisecond();

	play_list = ov::String::FormatString(_play_list.CStr(), current_time.CStr());

	return true;
}
