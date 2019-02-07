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

 	void convert_to(MediaPacket *packet);
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


#if 0
// TODO NAL_TYPE 값 정의
/ @srs_kernel_codec;
enum SrsAvcNaluType
{
    // Unspecified
    SrsAvcNaluTypeReserved = 0,

    // Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeNonIDR = 1,
    // Coded slice data partition A slice_data_partition_a_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionA = 2,
    // Coded slice data partition B slice_data_partition_b_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionB = 3,
    // Coded slice data partition C slice_data_partition_c_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionC = 4,
    // Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeIDR = 5,
    // Supplemental enhancement information (SEI) sei_rbsp( )
    SrsAvcNaluTypeSEI = 6,
    // Sequence parameter set seq_parameter_set_rbsp( )
    SrsAvcNaluTypeSPS = 7,
    // Picture parameter set pic_parameter_set_rbsp( )
    SrsAvcNaluTypePPS = 8,
    // Access unit delimiter access_unit_delimiter_rbsp( )
    SrsAvcNaluTypeAccessUnitDelimiter = 9,
    // End of sequence end_of_seq_rbsp( )
    SrsAvcNaluTypeEOSequence = 10,
    // End of stream end_of_stream_rbsp( )
    SrsAvcNaluTypeEOStream = 11,
    // Filler data filler_data_rbsp( )
    SrsAvcNaluTypeFilterData = 12,
    // Sequence parameter set extension seq_parameter_set_extension_rbsp( )
    SrsAvcNaluTypeSPSExt = 13,
    // Prefix NAL unit prefix_nal_unit_rbsp( )
    SrsAvcNaluTypePrefixNALU = 14,
    // Subset sequence parameter set subset_seq_parameter_set_rbsp( )
    SrsAvcNaluTypeSubsetSPS = 15,
    // Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeLayerWithoutPartition = 19,
    // Coded slice extension slice_layer_extension_rbsp( )
    SrsAvcNaluTypeCodedSliceExt = 20,
};

std::string srs_codec_avc_nalu2str(SrsAvcNaluType nalu_type);

string srs_codec_avc_nalu2str(SrsAvcNaluType nalu_type)
{
    switch (nalu_type) {
        case SrsAvcNaluTypeNonIDR: return "NonIDR";
        case SrsAvcNaluTypeDataPartitionA: return "DataPartitionA";
        case SrsAvcNaluTypeDataPartitionB: return "DataPartitionB";
        case SrsAvcNaluTypeDataPartitionC: return "DataPartitionC";
        case SrsAvcNaluTypeIDR: return "IDR";
        case SrsAvcNaluTypeSEI: return "SEI";
        case SrsAvcNaluTypeSPS: return "SPS";
        case SrsAvcNaluTypePPS: return "PPS";
        case SrsAvcNaluTypeAccessUnitDelimiter: return "AccessUnitDelimiter";
        case SrsAvcNaluTypeEOSequence: return "EOSequence";
        case SrsAvcNaluTypeEOStream: return "EOStream";
        case SrsAvcNaluTypeFilterData: return "FilterData";
        case SrsAvcNaluTypeSPSExt: return "SPSExt";
        case SrsAvcNaluTypePrefixNALU: return "PrefixNALU";
        case SrsAvcNaluTypeSubsetSPS: return "SubsetSPS";
        case SrsAvcNaluTypeLayerWithoutPartition: return "LayerWithoutPartition";
        case SrsAvcNaluTypeCodedSliceExt: return "CodedSliceExt";
        case SrsAvcNaluTypeReserved: default: return "Other";
    }
}
#endif