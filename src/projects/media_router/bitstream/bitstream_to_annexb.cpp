//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <stdio.h>
#include <string.h>

#include "bitstream_to_annexb.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "BitstreamToAnnexB"

BitstreamToAnnexB::BitstreamToAnnexB()
{
}

BitstreamToAnnexB::~BitstreamToAnnexB()
{
}

// NALU is basic unit.
// Then,
// annexb format:
// ([start code] NALU) | ( [start code] NALU) |
// avcc format:
// ([extradata]) | ([length] NALU) | ([length] NALU) |
// In annexb, [start code] may be 0x000001 or 0x00000001.

void BitstreamToAnnexB::convert_to(MediaPacket *packet)
{
	auto &data = packet->GetData();

	if(data->GetLength() < 4)
	{
		logtw("Could not determine bit stream type");
		return;
	}

	const uint8_t *pbuf = data->GetDataAs<uint8_t>();

	// Check if the bitstream is Annex-B type
	if(
		(pbuf[0] == 0x00) &&
		(pbuf[1] == 0x00) &&
		((pbuf[2] == 0x01) || (pbuf[2] == 0x00 && pbuf[3] == 0x01))
		)
	{
		logtd("already annexb type");
		return;
	}

	uint8_t start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
	auto pbuf_length = static_cast<uint32_t>(data->GetLength());

	// Convert [FLV] to [Video data]
	// Refrence: Video File Format Specification, Version 10 (https://wwwimages2.adobe.com/content/dam/acom/en/devnet/flv/video_file_format_spec_v10.pdf)

	// [Video data]
	//   4bit - frame_type
	//     1: key frame (for AVC)
	//     2: inter frame(for AVC)
	//     3: disposable inter frame(H.263)
	//     4: genreated keyframe(reserved for server use only)
	//     5: video info/command frame
	int frame_type = pbuf[0] >> 4 & 0x0f;
	//   4bit - codec_id
	//     1: JPEG
	//     2: Soenson H.263
	//     3: Screen video
	//     4: On2 VP6
	//     5: On2 VP7 with alpha channel
	//     6: Screen video version 2
	//     7: AVC
	int codec_id = pbuf[0] & 0x0f;
	//   8bit : AVCPacketType
	//     0: AVC Sequence header
	//     1: AVC NALU
	//     2: AVC end of sequence(lower level NALU sequence ender is not requered or supported)
	int avc_packet_type = 0;
	//   24bit - composition time
	// if(AVC Packet Type == 1)
	// Composition time offset
	// else
	// 0
	int composition_time = 0;

	if(codec_id == 7) // avcvideopacket
	{
		// 3byte
		avc_packet_type = pbuf[1];
		if(avc_packet_type == 1)
		{
			composition_time = pbuf[2] << 16 | pbuf[3] << 8 | pbuf[4];
		}
	}

	// logtd("size : %d, time = %.0f - frame_type=%d, codec_id=%d, avc_packet_type=%d, composition_time=%d", 
	// pkt->GetDataSize(), (float)pkt->GetPts(), frame_type, codec_id, avc_packet_type, composition_time);


	// if(avc_packet_type == 0) AVCDecoderConfigurationRecord
	if(frame_type == 1 && avc_packet_type == 0)
	{
		// logtd("%s", ov::Dump(pbuf, 256).CStr());

		const uint8_t *p = pbuf + 5;

		logtd("configurationVersion = %d", *(p++));
		logtd("AVCProfileIndication = %d", *(p++));
		logtd("profile_compatibility = %d", *(p++));
		logtd("AVCLevelIndication = %d", *(p++));
		logtd("lengthSizeMinusOne = %d", *(p++) & 0x03);

		logtd("numOfSequenceParameterSets = reserve(%02x) + %d", *p & 0xE0, *p & 0x1F); // reserve(3) + unsigned int(5)
		int numOfSequenceParameterSets = *p & 0x1F;
		p++;
		for(int i = 0; i < numOfSequenceParameterSets; i++)
		{
			int sequenceParameterSetLength = *p << 8 | *(p + 1);
			logtd(" sequenceParameterSetLength : %d", sequenceParameterSetLength);
			p += 2;

			// Utils::Debug::DumpHex(p, sequenceParameterSetLength);

			_sps.clear();
			_sps.insert(_sps.end(), p, p + sequenceParameterSetLength);

			p += sequenceParameterSetLength;
		}

		logtd("numOfPictureParameterSets = %d", *p); // reserve(3) + unsigned int(5)
		int numOfPictureParameterSets = *p & 0x1F;
		p++;
		for(int i = 0; i < numOfPictureParameterSets; i++)
		{
			int pictureParameterSetLength = *p << 8 | *(p + 1);
			logtd(" pictureParameterSetLength : %d", pictureParameterSetLength);
			p += 2;
			// Utils::Debug::DumpHex(p, pictureParameterSetLength);

			_pps.clear();
			_pps.insert(_pps.end(), p, p + pictureParameterSetLength);

			p += pictureParameterSetLength;
		}

		data->Clear();
		data->Append(start_code, 4);
		data->Append(_sps.data(), _sps.size());

		data->Append(start_code, 4);
		data->Append(_pps.data(), _pps.size());

		logtd("sps/pps packet size : %d", data->GetLength());
	}
		// NAL Unit
		/*
		0	Unspecified		non-VCL	non-VCL	non-VCL
		1	Coded slice of a non-IDR picture
		slice_layer_without_partitioning_rbsp( )	2, 3, 4	VCL	VCL	VCL
		2	Coded slice data partition A
		slice_data_partition_a_layer_rbsp( )	2	VCL	not applicable	not applicable
		3	Coded slice data partition B
		slice_data_partition_b_layer_rbsp( )	3	VCL	not applicable	not applicable
		4	Coded slice data partition C
		slice_data_partition_c_layer_rbsp( )	4	VCL	not applicable	not applicable
		5	Coded slice of an IDR picture
		slice_layer_without_partitioning_rbsp( )	2, 3	VCL	VCL	VCL
		6	Supplemental enhancement information (SEI)
		sei_rbsp( )	5	non-VCL	non-VCL	non-VCL
		7	Sequence parameter set
		seq_parameter_set_rbsp( )	0	non-VCL	non-VCL	non-VCL
		8	Picture parameter set
		pic_parameter_set_rbsp( )	1	non-VCL	non-VCL	non-VCL
		9	Access unit delimiter
		access_unit_delimiter_rbsp( )	6	non-VCL	non-VCL	non-VCL
		10	End of sequence
		end_of_seq_rbsp( )	7	non-VCL	non-VCL	non-VCL
		11	End of stream
		end_of_stream_rbsp( )	8	non-VCL	non-VCL	non-VCL
		12	Filler data
		filler_data_rbsp( )	9	non-VCL	non-VCL	non-VCL
		13	Sequence parameter set extension
		seq_parameter_set_extension_rbsp( )	10	non-VCL	non-VCL	non-VCL
		14	Prefix NAL unit
		prefix_nal_unit_rbsp( )	2	non-VCL	suffix dependent	suffix dependent
		15	Subset sequence parameter set
		subset_seq_parameter_set_rbsp( )	0	non-VCL	non-VCL	non-VCL
		16 – 18	Reserved		non-VCL	non-VCL	non-VCL
		19	Coded slice of an auxiliary coded picture without partitioning
		slice_layer_without_partitioning_rbsp( )	2, 3, 4	non-VCL	non-VCL	non-VCL
		20	Coded slice extension
		slice_layer_extension_rbsp( )	2, 3, 4	non-VCL	VCL	VCL
		21	Coded slice extension for depth view components
		slice_layer_extension_rbsp( )
		(specified in Annex I)	2, 3, 4	non-VCL	non-VCL	VCL
		22 – 23	Reserved		non-VCL	non-VCL	VCL
		24 – 31	Unspecified		non-VCL	non-VCL	non-VCL
		*/
	else if(frame_type == 1 && avc_packet_type == 1)
	{
		const uint8_t *p = pbuf + 5;

		auto tmp_buf = std::make_shared<MediaFrame>();

		while(1)
		{
			int32_t nal_length = static_cast<int32_t>(static_cast<uint8_t>(*(p)) << 24 |
			                                          static_cast<uint8_t>(*(p + 1)) << 16 |
			                                          static_cast<uint8_t>(*(p + 2)) << 8 |
			                                          static_cast<uint8_t>(*(p + 3)));

			// skip length 32bit
			p += 4;

			// logtd("nal_unit_type : %s", Nalu2Str( (AvcNaluType)((*p) & 0x1F) ).c_str());

			tmp_buf->AppendBuffer(start_code, 4);
			tmp_buf->AppendBuffer(p, nal_length);

			p += nal_length;
			if((p - pbuf) >= pbuf_length)
			{
				break;
			}
		}

		data->Clear();
		data->Append(tmp_buf->GetBuffer(), tmp_buf->GetBufferSize());
	}
	else if(frame_type == 1 && avc_packet_type == 2)
	{
		// logtd("frame_type(%d), avc_packet_type(%d)", frame_type, avc_packet_type);
		data->Clear();
		data->Append(start_code, 4);
	}
		// intra-frame
	else
	{
		const uint8_t *p = pbuf + 5;

		auto tmp_buf = std::make_shared<MediaFrame>();

		while(1)
		{
			int32_t nal_length = static_cast<int32_t>(static_cast<uint8_t>(*(p)) << 24 |
			                                          static_cast<uint8_t>(*(p + 1)) << 16 |
			                                          static_cast<uint8_t>(*(p + 2)) << 8 |
			                                          static_cast<uint8_t>(*(p + 3)));

			// logte("nal_lengh = %d, pbuf_length = %d", nal_length, pbuf_length);

			// skip length 32bit
			p += 4;

			// logtd("nal_unit_type : %s", Nalu2Str( (AvcNaluType)((*p) & 0x1F) ).c_str());

			tmp_buf->AppendBuffer(start_code, 4);
			tmp_buf->AppendBuffer(p, nal_length);

			p += nal_length;
			if((p - pbuf) >= pbuf_length)
			{
				break;
			}
		}

		data->Clear();
		data->Append(tmp_buf->GetBuffer(), tmp_buf->GetBufferSize());
	}
}

std::string BitstreamToAnnexB::Nalu2Str(AvcNaluType nalu_type)
{
	switch(nalu_type)
	{
		case AvcNaluTypeNonIDR:
			return "NonIDR";
		case AvcNaluTypeDataPartitionA:
			return "DataPartitionA";
		case AvcNaluTypeDataPartitionB:
			return "DataPartitionB";
		case AvcNaluTypeDataPartitionC:
			return "DataPartitionC";
		case AvcNaluTypeIDR:
			return "IDR";
		case AvcNaluTypeSEI:
			return "SEI";
		case AvcNaluTypeSPS:
			return "SPS";
		case AvcNaluTypePPS:
			return "PPS";
		case AvcNaluTypeAccessUnitDelimiter:
			return "AccessUnitDelimiter";
		case AvcNaluTypeEOSequence:
			return "EOSequence";
		case AvcNaluTypeEOStream:
			return "EOStream";
		case AvcNaluTypeFilterData:
			return "FilterData";
		case AvcNaluTypeSPSExt:
			return "SPSExt";
		case AvcNaluTypePrefixNALU:
			return "PrefixNALU";
		case AvcNaluTypeSubsetSPS:
			return "SubsetSPS";
		case AvcNaluTypeLayerWithoutPartition:
			return "LayerWithoutPartition";
		case AvcNaluTypeCodedSliceExt:
			return "CodedSliceExt";
		case AvcNaluTypeReserved:
		default:
			return "Other";
	}
}

//====================================================================================================
// SequenceHeaderParsing
// - Bitstream(Rtmp Input Low Data) Sequence Info Parsing
//====================================================================================================
bool BitstreamToAnnexB::SequenceHeaderParsing(const uint8_t *data,
                                                int data_size,
                                                std::vector<uint8_t> &sps,
                                                std::vector<uint8_t> &pps,
                                                uint8_t &avc_profile,
                                                uint8_t &avc_profile_compatibility,
                                                uint8_t &avc_level)
{
	if(data_size < 4)
	{
		logtw("Could not determine bit stream type");
		return false;
	}

	int frame_type = data[0] >> 4 & 0x0f;
	int codec_id = data[0] & 0x0f;
	int avc_packet_type = data[1];

	if(frame_type != 1 || codec_id != 7 || avc_packet_type != 0) // avcvideopacket
	{
		return false;
	}

	data += 5;
	logtd("configuration version = %d", *(data++));
	avc_profile =  *(data++);
	avc_profile_compatibility = *(data++);
	avc_level = *(data++);
	logtd("lengthSizeMinusOne = %d", *(data++) & 0x03);

	logtd("sps count = reserve(%02x) + %d", *data & 0xE0, *data & 0x1F); // reserve(3) + unsigned int(5)
	int sps_count = *data & 0x1F;
	data++;
	for(int index = 0; index < sps_count; index++)
	{
		int sps_size = *data << 8 | *(data + 1);
		data += 2;
		// Utils::Debug::DumpHex(p, sps_size);
		sps.clear();
		sps.insert(sps.end(), data, data + sps_size);
		data += sps_size;
	}

	logtd("pps count = %d", *data); // reserve(3) + unsigned int(5)
	int pps_count = *data & 0x1F;
	data++;
	for(int index = 0; index < pps_count; index++)
	{
		int pps_size = *data << 8 | *(data + 1);
		data += 2;
		// Utils::Debug::DumpHex(p, pps_size);
		pps.clear();
		pps.insert(pps.end(), data, data + pps_size);
		data += pps_size;
	}

	return true;
}
