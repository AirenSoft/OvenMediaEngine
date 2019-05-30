//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include <stdint.h>

class BitstreamToAnnexB
{
public:
	enum AvcNaluType
	{
	    // Unspecified
	    AvcNaluTypeReserved = 0,

	    // Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
	    AvcNaluTypeNonIDR = 1,
	    // Coded slice data partition A slice_data_partition_a_layer_rbsp( )
	    AvcNaluTypeDataPartitionA = 2,
	    // Coded slice data partition B slice_data_partition_b_layer_rbsp( )
	    AvcNaluTypeDataPartitionB = 3,
	    // Coded slice data partition C slice_data_partition_c_layer_rbsp( )
	    AvcNaluTypeDataPartitionC = 4,
	    // Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
	    AvcNaluTypeIDR = 5,
	    // Supplemental enhancement information (SEI) sei_rbsp( )
	    AvcNaluTypeSEI = 6,
	    // Sequence parameter set seq_parameter_set_rbsp( )
	    AvcNaluTypeSPS = 7,
	    // Picture parameter set pic_parameter_set_rbsp( )
	    AvcNaluTypePPS = 8,
	    // Access unit delimiter access_unit_delimiter_rbsp( )
	    AvcNaluTypeAccessUnitDelimiter = 9,
	    // End of sequence end_of_seq_rbsp( )
	    AvcNaluTypeEOSequence = 10,
	    // End of stream end_of_stream_rbsp( )
	    AvcNaluTypeEOStream = 11,
	    // Filler data filler_data_rbsp( )
	    AvcNaluTypeFilterData = 12,
	    // Sequence parameter set extension seq_parameter_set_extension_rbsp( )
	    AvcNaluTypeSPSExt = 13,
	    // Prefix NAL unit prefix_nal_unit_rbsp( )
	    AvcNaluTypePrefixNALU = 14,
	    // Subset sequence parameter set subset_seq_parameter_set_rbsp( )
	    AvcNaluTypeSubsetSPS = 15,
	    // Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
	    AvcNaluTypeLayerWithoutPartition = 19,
	    // Coded slice extension slice_layer_extension_rbsp( )
	    AvcNaluTypeCodedSliceExt = 20,
	};

	std::string Nalu2Str(AvcNaluType nalu_type);
public:
    BitstreamToAnnexB();
    ~BitstreamToAnnexB();

	void convert_to(const std::shared_ptr<ov::Data> &data, int64_t &cts);
 	static bool SequenceHeaderParsing(const uint8_t *data,
                                        int data_size,
                                        std::vector<uint8_t> &_sps,
                                        std::vector<uint8_t> &_pps,
                                        uint8_t &avc_profile,
                                        uint8_t &avc_profile_compatibility,
                                        uint8_t &avc_level);

private:

	std::vector<uint8_t> 	_sps;
	std::vector<uint8_t> 	_pps;
};
