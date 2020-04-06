#pragma once

#include <base/common_types.h>
#include "base/media_route/media_buffer.h"

#include <cstdint>
#include <memory>

class AvcVideoPacketFragmentizer
{
public:
	bool MakeHeader(const std::shared_ptr<MediaPacket> &packet);

	// There is a duplicate code.
	// 	- @bitstream_to_annexb header
	enum class AvcNaluType : uint8_t
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

private:
    uint8_t nal_length_size_ = 0;
    size_t continuation_length_ = 0;
};