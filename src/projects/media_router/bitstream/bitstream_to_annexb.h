//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>

class BitstreamToAnnexB
{
public:
	enum class AvcFrameType : char
	{
		// 1: keyframe (for AVC, a seekable frame)
		Key = 1,
		// 2: inter frame (for AVC, a non-seekable frame)
		Inter = 2,
		// 3: disposable inter frame (H.263 only)
		DisposableInter = 3,
		// 4: generated keyframe (reserved for server use only)
		GeneratedKey = 4,
		// 5: video info/command frame
		VideoInfo = 5
	};

	enum class AvcCodecId : char
	{
		// 1: JPEG (currently unused)
		Jpeg = 1,
		// 2: Sorenson H.263
		SorensonH263 = 2,
		// 3: Screen video
		ScreenVideo = 3,
		// 4: On2 VP6
		On2Vp6 = 4,
		// 5: On2 VP6 with alpha channel
		On2Vp6Alpha = 5,
		// 6: Screen video version 2
		ScreenVideoV2 = 6,
		// 7: AVC
		AVC = 7
	};

	enum class AvcPacketType : char
	{
		// 0: AVC sequence header
		SeqHeader = 0,
		// 1: AVC NALU
		Nalu = 1,
		// 2: AVC end of sequence (lower level NALU sequence ender is not required or supported)
		EndOfSeq = 2
	};

	enum class AvcNaluType : char
	{
		// Unspecified
		Reserved = 0,

		// Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
		NonIDR = 1,
		// Coded slice data partition A slice_data_partition_a_layer_rbsp( )
		DataPartitionA = 2,
		// Coded slice data partition B slice_data_partition_b_layer_rbsp( )
		DataPartitionB = 3,
		// Coded slice data partition C slice_data_partition_c_layer_rbsp( )
		DataPartitionC = 4,
		// Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
		IDR = 5,
		// Supplemental enhancement information (SEI) sei_rbsp( )
		SEI = 6,
		// Sequence parameter set seq_parameter_set_rbsp( )
		SPS = 7,
		// Picture parameter set pic_parameter_set_rbsp( )
		PPS = 8,
		// Access unit delimiter access_unit_delimiter_rbsp( )
		AccessUnitDelimiter = 9,
		// End of sequence end_of_seq_rbsp( )
		EOSequence = 10,
		// End of stream end_of_stream_rbsp( )
		EOStream = 11,
		// Filler data filler_data_rbsp( )
		FilterData = 12,
		// Sequence parameter set extension seq_parameter_set_extension_rbsp( )
		SPSExt = 13,
		// Prefix NAL unit prefix_nal_unit_rbsp( )
		PrefixNALU = 14,
		// Subset sequence parameter set subset_seq_parameter_set_rbsp( )
		SubsetSPS = 15,
		// Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
		LayerWithoutPartition = 19,
		// Coded slice extension slice_layer_extension_rbsp( )
		CodedSliceExt = 20,
	};

	BitstreamToAnnexB() = default;
	~BitstreamToAnnexB() = default;

	// Convert AnnexB format to AVCC format
	//   AnnexB format: ([start code] NALU) | ([start code] NALU) | ...
	//     [start code] = 0x000001 or 0x00000001
	//   AVCC format:   ([extra data]) | ([length] NALU) | ([length] NALU) |
	bool Convert(const std::shared_ptr<ov::Data> &data, int64_t &cts);

	static bool ParseSequenceHeader(const uint8_t *data,
	                                int data_size,
	                                std::vector<uint8_t> &_sps,
	                                std::vector<uint8_t> &_pps,
	                                uint8_t &avc_profile,
	                                uint8_t &avc_profile_compatibility,
	                                uint8_t &avc_level);

private:
	bool IsAnnexB(const uint8_t *buffer) const;

	bool ConvertKeyFrame(AvcPacketType packet_type, ov::ByteStream read_stream, const std::shared_ptr<ov::Data> &data);
	bool ConvertInterFrame(AvcPacketType packet_type, ov::ByteStream read_stream, const std::shared_ptr<ov::Data> &data);

	std::shared_ptr<const ov::Data> _sps;
	std::shared_ptr<const ov::Data> _pps;

	std::vector<uint8_t> _old_sps;
	std::vector<uint8_t> _old_pps;
};
