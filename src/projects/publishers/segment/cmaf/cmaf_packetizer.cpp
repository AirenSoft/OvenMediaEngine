//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_packetizer.h"

#include "cmaf_private.h"
// TODO(dimiden): Merge DASH and CMAF module later
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

#include "../dash/dash_define.h"

// Unit: ms
#define CMAF_JITTER_THRESHOLD 2000
#define CMAF_JITTER_CORRECTION_PER_ONCE 1000
#define CMAF_JITTER_CORRECTION_INTERVAL 5000

CmafPacketizer::CmafPacketizer(const ov::String &app_name, const ov::String &stream_name,
							   uint32_t segment_count, uint32_t segment_duration,
							   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
							   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Packetizer(app_name, stream_name,
				 1, 1 * 5, segment_duration,
				 video_track, audio_track,

				 chunked_transfer)
{
	_mpd_min_buffer_time = 6;

	if (video_track != nullptr)
	{
		uint32_t resolution_gcd = std::gcd(video_track->GetWidth(), video_track->GetHeight());

		if (resolution_gcd != 0)
		{
			std::ostringstream pixel_aspect_ratio;
			pixel_aspect_ratio << video_track->GetWidth() / resolution_gcd << ":"
							   << video_track->GetHeight() / resolution_gcd;

			_pixel_aspect_ratio = pixel_aspect_ratio.str().c_str();
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

	if (_video_track != nullptr)
	{
		_video_chunk_writer = std::make_shared<CmafChunkWriter>(M4sMediaType::Video, 1, _ideal_duration_for_video);
	}

	if (_audio_track != nullptr)
	{
		_audio_chunk_writer = std::make_shared<CmafChunkWriter>(M4sMediaType::Audio, 2, _ideal_duration_for_audio);
	}
}

DashFileType CmafPacketizer::GetFileType(const ov::String &file_name)
{
	if (file_name == CMAF_MPD_VIDEO_FULL_INIT_FILE_NAME)
	{
		return DashFileType::VideoInit;
	}
	else if (file_name == CMAF_MPD_AUDIO_FULL_INIT_FILE_NAME)
	{
		return DashFileType::AudioInit;
	}
	else if (file_name.IndexOf(CMAF_MPD_VIDEO_FULL_SUFFIX) >= 0)
	{
		return DashFileType::VideoSegment;
	}
	else if (file_name.IndexOf(CMAF_MPD_AUDIO_FULL_SUFFIX) >= 0)
	{
		return DashFileType::AudioSegment;
	}

	return DashFileType::Unknown;
}

int CmafPacketizer::GetStartPatternSize(const uint8_t *buffer, const size_t buffer_len)
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

ov::String CmafPacketizer::GetFileName(cmn::MediaType media_type) const
{
	switch (media_type)
	{
		case cmn::MediaType::Video:
			return ov::String::FormatString("%u%s", _video_chunk_writer->GetSequenceNumber(), CMAF_MPD_VIDEO_FULL_SUFFIX);

		case cmn::MediaType::Audio:
			return ov::String::FormatString("%u%s", _audio_chunk_writer->GetSequenceNumber(), CMAF_MPD_AUDIO_FULL_SUFFIX);

		default:
			break;
	}

	return "";
}

static inline void DumpSegmentToFile(const std::shared_ptr<const SegmentItem> &segment_item)
{
#if DEBUG
#	if 0
	auto &file_name = segment_item->file_name;
	auto &data = segment_item->data;

	ov::DumpToFile(ov::PathManager::Combine(ov::PathManager::GetAppPath("dump/lldash"), file_name), data);
#	endif
#endif	// DEBUG
}

bool CmafPacketizer::WriteVideoInitInternal(const std::shared_ptr<const ov::Data> &frame, const ov::String &init_file_name)
{
	const uint8_t *srcData = frame->GetDataAs<uint8_t>();
	size_t dataOffset = 0;
	size_t dataSize = frame->GetLength();

	std::vector<std::pair<size_t, size_t>> offset_list;

	// int total_start_pattern_size = 0;
	int nal_packet_header_length = 3;

	// Stage 1 - Extract the Offset and Lengh value of the NAL Packet

	while (dataOffset < dataSize)
	{
		size_t remainDataSize = dataSize - dataOffset;
		const uint8_t *data = srcData + dataOffset;

		if (remainDataSize >= 3 && 0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2])
		{
			nal_packet_header_length = 3;
			offset_list.emplace_back(dataOffset, 3);  // Offset, SIZEOF(START_CODE[3])
			dataOffset += 3;
		}
		else if (remainDataSize >= 4 && 0x00 == data[0] && 0x00 == data[1] && 0x00 == data[2] && 0x01 == data[3])
		{
			nal_packet_header_length = 4;
			offset_list.emplace_back(dataOffset, 4);  // Offset, SIZEOF(START_CODE[4])
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
			nalu_data_len = offset_list[index + 1].first - nalu_offset;
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
		uint8_t nal_unit_type = (nalu_header)&0x01F;

#if 0  // for debug
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

	if (!(nal_packet_header_length == 3 || nal_packet_header_length == 4))
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
	_video_init_file = std::make_shared<SegmentItem>(SegmentDataType::Video, 0, init_file_name, 0, 0, 0, 0, init_data);

	DumpSegmentToFile(_video_init_file);

	logtd("%s init file (%s) is written for video [%s/%s]", GetPacketizerName(), init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool CmafPacketizer::WriteVideoInit(const std::shared_ptr<const ov::Data> &frame_data)
{
	return WriteVideoInitInternal(frame_data, CMAF_MPD_VIDEO_FULL_INIT_FILE_NAME);
}

bool CmafPacketizer::WriteAudioInit(const std::shared_ptr<const ov::Data> &frame_data)
{
	return WriteAudioInitInternal(frame_data, CMAF_MPD_AUDIO_FULL_INIT_FILE_NAME);
}

bool CmafPacketizer::AppendVideoFrameInternal(const std::shared_ptr<const PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback)
{
	if (WriteVideoInitIfNeeded(frame) == false)
	{
		return false;
	}

	auto data = frame->data;

	// Calculate offset to skip NAL header
	int offset = (frame->type == PacketizerFrameType::VideoKeyFrame) ? _avc_nal_header_size : GetStartPatternSize(data->GetDataAs<uint8_t>(), data->GetLength());

	if (static_cast<int>(data->GetLength()) < offset)
	{
		// Not enough data
		logtw("Invalid frame: frame is too short: expected: %d, but %zu bytes", offset, data->GetLength());
		return false;
	}

	if (frame->type == PacketizerFrameType::VideoKeyFrame)
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
	auto sample_data = std::make_shared<SampleData>(frame->duration, flag, frame->pts, frame->dts, data);

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

bool CmafPacketizer::AppendAudioFrameInternal(const std::shared_ptr<const PacketizerFrameData> &frame, uint64_t current_segment_duration, DataCallback data_callback)
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
	auto data = frame->data->Subdata(ADTS_HEADER_SIZE);

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
			data_callback(std::make_shared<SampleData>(frame->duration, frame->pts, frame->dts, data), new_segment_written);
		}
	}

	return true;
}
bool CmafPacketizer::AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &frame)
{
	return AppendVideoFrameInternal(frame, _video_chunk_writer->GetSegmentDuration(), [this](const std::shared_ptr<const SampleData> data, bool new_segment_written) {
		auto chunk_data = _video_chunk_writer->AppendSample(data);

		if (chunk_data != nullptr && _chunked_transfer != nullptr)
		{
			// Response chunk data to HTTP client
			_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetFileName(cmn::MediaType::Video), true, chunk_data);
		}

		_last_video_pts = data->pts;

		if (_first_video_pts == -1LL)
		{
			_first_video_pts = _last_video_pts;
		}
	});
}

bool CmafPacketizer::WriteAudioInitInternal(const std::shared_ptr<const ov::Data> &frame, const ov::String &init_file_name)
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
	_audio_init_file = std::make_shared<SegmentItem>(SegmentDataType::Audio, 0, init_file_name, 0, 0, 0, 0, init_data);

	DumpSegmentToFile(_audio_init_file);

	logtd("%s init file (%s) is written for audio [%s/%s]", GetPacketizerName(), init_file_name.CStr(), _app_name.CStr(), _stream_name.CStr());

	return true;
}

bool CmafPacketizer::AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &frame)
{
	return AppendAudioFrameInternal(frame, _audio_chunk_writer->GetSegmentDuration(), [this](const std::shared_ptr<const SampleData> data, bool new_segment_written) {
		auto chunk_data = _audio_chunk_writer->AppendSample(data);

		if (chunk_data != nullptr && _chunked_transfer != nullptr)
		{
			// Response chunk data to HTTP client
			_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetFileName(cmn::MediaType::Audio), false, chunk_data);
		}

		_last_audio_pts = data->pts;

		if (_first_audio_pts == -1LL)
		{
			_first_audio_pts = _last_audio_pts;
		}
	});
}

bool CmafPacketizer::WriteVideoSegment()
{
	if (_video_chunk_writer->GetSampleCount() == 0)
	{
		logtd("There is no video data for CMAF segment");
		return true;
	}

	auto file_name = GetFileName(cmn::MediaType::Video);
	auto start_timestamp = _video_chunk_writer->GetStartTimestamp();
	auto segment_duration = _video_chunk_writer->GetSegmentDuration();

	// Create a fragment
	auto segment_data = _video_chunk_writer->GetChunkedSegment();
	_video_chunk_writer->Clear();

	// Enqueue the segment
	// TODO: need to change start_timestamp/segment_duration to start_timestamp_in_ms/segment_duration_in_ms
	if (SetSegmentData(file_name, start_timestamp, start_timestamp, segment_duration, segment_duration, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_video += (_ideal_duration_for_video - segment_duration);

	if (_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, true);
	}

	return true;
}

bool CmafPacketizer::WriteAudioSegment()
{
	if (_audio_chunk_writer->GetSampleCount() == 0)
	{
		logtd("There is no audio data for CMAF segment");
		return true;
	}

	auto file_name = GetFileName(cmn::MediaType::Audio);
	auto start_timestamp = _audio_chunk_writer->GetStartTimestamp();
	auto segment_duration = _audio_chunk_writer->GetSegmentDuration();

	// Create a fragment
	auto segment_data = _audio_chunk_writer->GetChunkedSegment();
	_audio_chunk_writer->Clear();

	// Enqueue the segment
	if (SetSegmentData(file_name, start_timestamp, start_timestamp, segment_duration, segment_duration, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_audio += (_ideal_duration_for_audio - segment_duration);

	if (_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, false);
	}

	return true;
}

bool CmafPacketizer::WriteVideoInitIfNeeded(const std::shared_ptr<const PacketizerFrameData> &frame)
{
	if (_video_key_frame_received)
	{
		return true;
	}

	if ((frame->type == PacketizerFrameType::VideoKeyFrame) && WriteVideoInit(frame->data))
	{
		_video_key_frame_received = true;
	}

	return _video_key_frame_received;
}

bool CmafPacketizer::WriteAudioInitIfNeeded(const std::shared_ptr<const PacketizerFrameData> &frame)
{
	if (_audio_key_frame_received)
	{
		return true;
	}

	_audio_key_frame_received = WriteAudioInit(frame->data);

	return _audio_key_frame_received;
}

std::shared_ptr<const SegmentItem> CmafPacketizer::GetSegmentData(const ov::String &file_name) const
{
	if (IsReadyForStreaming() == false)
	{
		logtd("[%p] Could not obtain segment data for %s, stream is not started", this, file_name.CStr());

		return nullptr;
	}

	auto file_type = GetFileType(file_name);

	switch (file_type)
	{
		case DashFileType::VideoSegment: {
			std::unique_lock<std::mutex> lock(_video_segment_mutex);

			auto item = std::find_if(_video_segments.begin(), _video_segments.end(),
									 [&](std::shared_ptr<SegmentItem> const &value) -> bool {
										 return value != nullptr ? value->file_name == file_name : false;
									 });

			return (item != _video_segments.end()) ? (*item) : nullptr;
		}

		case DashFileType::AudioSegment: {
			std::unique_lock<std::mutex> lock(_audio_segment_mutex);

			auto item = std::find_if(_audio_segments.begin(), _audio_segments.end(),
									 [&](std::shared_ptr<SegmentItem> const &value) -> bool {
										 return value != nullptr ? value->file_name == file_name : false;
									 });

			return (item != _audio_segments.end()) ? (*item) : nullptr;
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

bool CmafPacketizer::SetSegmentData(ov::String file_name, int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms, const std::shared_ptr<const ov::Data> &data)
{
	auto file_type = GetFileType(file_name);

	switch (file_type)
	{
		case DashFileType::VideoSegment: {
			// video segment mutex
			std::unique_lock<std::mutex> lock(_video_segment_mutex);
			auto segment = std::make_shared<SegmentItem>(SegmentDataType::Video, _sequence_number++, file_name, timestamp, timestamp_in_ms, duration, duration_in_ms, data);

			_video_segments[_current_video_index++] = segment;

			if (_segment_save_count <= _current_video_index)
			{
				_current_video_index = 0;
			}

			DumpSegmentToFile(segment);

			_video_segment_count++;

			logtd("%s segment is added for video stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  GetPacketizerName(), _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration_in_ms, data->GetLength(), duration_in_ms, _video_track->GetTimeBase().GetTimescale(), (double)duration_in_ms / _video_track->GetTimeBase().GetTimescale());

			break;
		}

		case DashFileType::AudioSegment: {
			// audio segment mutex
			std::unique_lock<std::mutex> lock(_audio_segment_mutex);
			auto segment = std::make_shared<SegmentItem>(SegmentDataType::Audio, _sequence_number++, file_name, timestamp, timestamp_in_ms, duration, duration_in_ms, data);

			_audio_segments[_current_audio_index++] = segment;

			if (_segment_save_count <= _current_audio_index)
			{
				_current_audio_index = 0;
			}

			DumpSegmentToFile(segment);

			_audio_segment_count++;

			logtd("%s segment is added for audio stream [%s/%s], file: %s, duration: %llu, size: %zu (scale: %llu/%.0f = %0.3f)",
				  GetPacketizerName(), _app_name.CStr(), _stream_name.CStr(), file_name.CStr(), duration_in_ms, data->GetLength(), duration_in_ms, _audio_track->GetTimeBase().GetTimescale(), (double)duration_in_ms / _audio_track->GetTimeBase().GetTimescale());

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

void CmafPacketizer::SetReadyForStreaming() noexcept
{
	_start_time_ms = std::max(_video_start_time, _audio_start_time);
	_start_time = MakeUtcMillisecond(_start_time_ms);

	Packetizer::SetReadyForStreaming();
}

ov::String CmafPacketizer::MakeJitterStatString(int64_t elapsed_time, int64_t current_time, int64_t jitter, int64_t adjusted_jitter, int64_t new_jitter_correction, int64_t video_delta, int64_t audio_delta, int64_t stream_delta) const
{
	ov::String stat;

	stat.AppendFormat(
		"  - Elapsed: %lldms (current: %lldms, start: %lldms)\n"
		"  - Jitter: %lldms (%lldms %c %lldms), correction: %lldms => %lldms (%lldms)\n"
		"  - Stream delta: %lldms",
		elapsed_time, current_time, _start_time_ms,
		adjusted_jitter, jitter, (_jitter_correction >= 0 ? '-' : '+'), std::abs(_jitter_correction), _jitter_correction, new_jitter_correction, (_jitter_correction - new_jitter_correction),
		stream_delta);

	if (_last_video_pts >= 0LL)
	{
		stat.AppendFormat(
			"\n    - Video: last PTS: %lldms, start: %lldms, delta: %lldms",
			static_cast<int64_t>(_last_video_pts * _video_scale), static_cast<int64_t>(_first_video_pts * _video_scale), video_delta);
	}

	if (_last_audio_pts >= 0LL)
	{
		stat.AppendFormat(
			"\n    - Audio: last PTS: %lldms, start: %lldms, delta: %lldms",
			static_cast<int64_t>(_last_audio_pts * _audio_scale), static_cast<int64_t>(_first_audio_pts * _audio_scale), audio_delta);
	}

	if ((_last_video_pts >= 0LL) && (_last_audio_pts >= 0LL))
	{
		stat.AppendFormat(
			"\n    - A-V Sync: %lld (A: %lld, V: %lld)",
			audio_delta - video_delta, audio_delta, video_delta);
	}

	return stat;
}

void CmafPacketizer::DoJitterCorrection()
{
	if (_stat_stop_watch.IsElapsed(CMAF_JITTER_CORRECTION_INTERVAL) == false)
	{
		// Do not need to correct jitter
		return;
	}

	_stat_stop_watch.Update();

	int64_t new_jitter_correction = _jitter_correction;
	ov::String stat;

	// Calculate total elapsed time since streaming started
	int64_t current_time = GetTimestampInMs();
	int64_t elapsed_time = current_time - _start_time_ms;

	int64_t video_delta = (_last_video_pts >= 0LL) ? (static_cast<int64_t>((_last_video_pts - _first_video_pts) * _video_scale)) : INT64_MAX;
	int64_t audio_delta = (_last_audio_pts >= 0LL) ? (static_cast<int64_t>((_last_audio_pts - _first_audio_pts) * _audio_scale)) : INT64_MAX;
	int64_t stream_delta = std::min(video_delta, audio_delta);

	int64_t jitter = elapsed_time - stream_delta;
	int64_t adjusted_jitter = jitter - _jitter_correction;

	if (adjusted_jitter > CMAF_JITTER_THRESHOLD)
	{
		new_jitter_correction += CMAF_JITTER_CORRECTION_PER_ONCE;
	}
	else if ((adjusted_jitter < 0L) && (_jitter_correction > 0L))
	{
		new_jitter_correction -= CMAF_JITTER_CORRECTION_PER_ONCE;
	}

	if (new_jitter_correction != _jitter_correction)
	{
		// Update start time
		ov::String new_start_time = MakeUtcMillisecond(_start_time_ms + new_jitter_correction);
		ov::String jitter_stat = MakeJitterStatString(elapsed_time, current_time, jitter, adjusted_jitter, new_jitter_correction, video_delta, audio_delta, stream_delta);

		if (new_jitter_correction > _jitter_correction)
		{
			logte("[%s/%s] Because the jitter is too high, playback may not be possible (%s => %s, %lldms)\n%s",
				  _app_name.CStr(), _stream_name.CStr(), _start_time.CStr(), new_start_time.CStr(), new_jitter_correction,
				  jitter_stat.CStr());
		}
		else
		{
			logtw("[%s/%s] Jitter has been reduced, but playback may not be possible (%s => %s, %lldms)\n%s",
				  _app_name.CStr(), _stream_name.CStr(), _start_time.CStr(), new_start_time.CStr(), new_jitter_correction,
				  jitter_stat.CStr());
		}

		_jitter_correction = new_jitter_correction;
		_start_time = std::move(new_start_time);
	}
	else
	{
		// not corrected
		logts("[%s/%s] Jitter statistics\n%s",
			  _app_name.CStr(), _stream_name.CStr(),
			  MakeJitterStatString(elapsed_time, current_time, jitter, adjusted_jitter, new_jitter_correction, video_delta, audio_delta, stream_delta).CStr());
	}
}

bool CmafPacketizer::UpdatePlayList()
{
	std::ostringstream xml;

	if (IsReadyForStreaming() == false)
	{
		return false;
	}

	DoJitterCorrection();

	ov::String publish_time = MakeUtcSecond(::time(nullptr));

	logtd("Trying to update playlist for LL-DASH with availabilityStartTime: %s, publishTime: %s", _start_time.CStr(), publish_time.CStr());

	xml << std::fixed << std::setprecision(3)
		<< R"(<?xml version="1.0" encoding="utf-8"?>)" << std::endl

		// MPD
		<< R"(<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance")" << std::endl
		<< R"(	xmlns="urn:mpeg:dash:schema:mpd:2011")" << std::endl
		<< R"(	xmlns:xlink="http://www.w3.org/1999/xlink")" << std::endl
		<< R"(	xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd")" << std::endl
		<< R"(	profiles="urn:mpeg:dash:profile:isoff-live:2011")" << std::endl
		<< R"(	type="dynamic")" << std::endl
		<< R"(	minimumUpdatePeriod="PT30S")" << std::endl
		<< R"(	publishTime=")" << publish_time.CStr() << R"(")" << std::endl
		<< R"(	availabilityStartTime=")" << _start_time.CStr() << R"(")" << std::endl
		<< R"(	timeShiftBufferDepth="PT)" << (_segment_save_count * _segment_duration) << R"(S")" << std::endl
		<< R"(	suggestedPresentationDelay="PT)" << _segment_duration << R"(S")" << std::endl
		<< R"(	minBufferTime="PT)" << _segment_duration << R"(S">)" << std::endl;

	{
		xml
			// <Period>
			<< R"(	<Period start="PT0.0S">)" << std::endl;

		if (_last_video_pts >= 0LL)
		{
			// availabilityTimeOffset
			//
			// - Proposed DASH extension
			// - Player usually sends request when entire segment available
			// - availabilityTimeOffset indicates difference of availabilityStartTime of the segment and UTC time
			//   of server when it can start delivering segment
			//
			// availabilityTimeOffset = [segment duration] - [chunk duration] - [UTC mismatch between server and client]

			// Reference: http://mile-high.video/files/mhv2018/pdf/day2/2_06_Henthorne.pdf

			double availability_time_offset = _video_track->GetFrameRate() != 0 ? (_segment_duration - (1.0 / _video_track->GetFrameRate())) : _segment_duration;

			xml
				// <AdaptationSet>
				<< R"(		<AdaptationSet contentType="video" segmentAlignment="true" )"
				<< R"(frameRate=")" << _video_track->GetFrameRate() << R"(">)" << std::endl;

			{
				xml
					// <Representation>
					<< R"(			<Representation id="0" mimeType="video/mp4" codecs="avc1.42401f" )"
					<< R"(bandwidth=")" << _video_track->GetBitrate() << R"(" )"
					<< R"(width=")" << _video_track->GetWidth() << R"(" )"
					<< R"(height=")" << _video_track->GetHeight() << R"(" )"
					<< R"(frameRate=")" << _video_track->GetFrameRate() << R"(">)" << std::endl;

				{
					xml
						// <SegmentTemplate />
						<< R"(				<SegmentTemplate startNumber="0" )"
						<< R"(timescale=")" << static_cast<uint32_t>(_video_track->GetTimeBase().GetTimescale()) << R"(" )"
						<< R"(duration=")" << static_cast<uint32_t>(_segment_duration * _video_track->GetTimeBase().GetTimescale()) << R"(" )"
						<< R"(availabilityTimeOffset=")" << availability_time_offset << R"(" )"
						<< R"(initialization=")" << CMAF_MPD_VIDEO_FULL_INIT_FILE_NAME << R"(" )"
						<< R"(media="$Number$)" << CMAF_MPD_VIDEO_FULL_SUFFIX << R"(" />)" << std::endl;
				}

				xml
					// </Representation>
					<< R"(			</Representation>)" << std::endl;
			}

			xml
				// </AdaptationSet>
				<< R"(		</AdaptationSet>)" << std::endl;
		}

		if (_last_audio_pts >= 0LL)
		{
			// segment duration - audio one frame duration
			double availability_time_offset = (_audio_track->GetSampleRate() / 1024) != 0 ? _segment_duration - (1.0 / ((double)_audio_track->GetSampleRate() / 1024)) : _segment_duration;

			xml
				// <AdaptationSet>
				<< R"(		<AdaptationSet contentType="audio" segmentAlignment="true">)" << std::endl;

			{
				xml
					// <Representation>
					<< R"(			<Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.2" )"
					<< R"(bandwidth=")" << _audio_track->GetBitrate() << R"(" )"
					<< R"(audioSamplingRate=")" << _audio_track->GetSampleRate() << R"(">)" << std::endl;

				{
					xml
						// <AudioChannelConfiguration />
						<< R"(				<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" )"
						<< R"(value=")" << _audio_track->GetChannel().GetCounts() << R"(" />)" << std::endl;

					xml
						// <SegmentTemplate />
						<< R"(				<SegmentTemplate startNumber="0" )"
						<< R"(timescale=")" << static_cast<uint32_t>(_audio_track->GetTimeBase().GetTimescale()) << R"(" )"
						<< R"(duration=")" << static_cast<uint32_t>(_segment_duration * _audio_track->GetTimeBase().GetTimescale()) << R"(" )"
						<< R"(availabilityTimeOffset=")" << availability_time_offset << R"(" )"
						<< R"(initialization=")" << CMAF_MPD_AUDIO_FULL_INIT_FILE_NAME << R"(" )"
						<< R"(media="$Number$)" << CMAF_MPD_AUDIO_FULL_SUFFIX << R"(" />)" << std::endl;
				}
				xml
					// </Representation>
					<< R"(			</Representation>)" << std::endl;
			}

			xml
				// </AdaptationSet>
				<< R"(		</AdaptationSet>)" << std::endl;
		}

		xml <<
			// </Period>
			R"(	</Period>)" << std::endl;
	}

	// The dash.js player does not normally recognize the milliseconds value of <UTCTiming>, causing a big difference between the server time and client time.
#if 0
	xml
		// <UTCTiming />
		<< R"(<UTCTiming schemeIdUri="urn:mpeg:dash:utc:direct:2014" value="%s" />)" << std::endl;
#endif

	xml
		// </MPD>
		<< R"(</MPD>)";

	ov::String play_list = xml.str().c_str();

	SetPlayList(play_list);

	return true;
}

bool CmafPacketizer::GetPlayList(ov::String &play_list)
{
	if (IsReadyForStreaming() == false)
	{
		logad("Manifest was requested before the stream began");
		return false;
	}

	ov::String current_time = MakeUtcMillisecond();

	play_list = ov::String::FormatString(_play_list.CStr(), current_time.CStr());

	return true;
}
