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

void BitstreamToAnnexB::convert_to(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	if(data->GetLength() < 4)
	{
		logtw("Could not determine bit stream type");
		return;
	}

	auto pbuf = data->GetDataAs<uint8_t>();

	// Check if the bitstream is Annex-B type
	if(
		(pbuf[0] == 0x00) &&
		(pbuf[1] == 0x00) &&
		((pbuf[2] == 0x01) || (pbuf[2] == 0x00 && pbuf[3] == 0x01))
		)
	{
		// The stream is already annexb type
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
	//int composition_time = 0;

	if(codec_id == 7) // avcvideopacket
	{
		// 3byte
		avc_packet_type = pbuf[1];
		if(avc_packet_type == 1)
		{
			//composition_time = pbuf[2] << 16 | pbuf[3] << 8 | pbuf[4];
		}
	}
	else
	{
		logte("Not supported codec: %d", codec_id);
		return;
	}

	switch(frame_type)
	{
		case 1:
		{
			// keyframe (for AVC, a seekable frame)
			switch(avc_packet_type)
			{
				case 0:
				{
					// AVC sequence header

					// pBuf[0] == CodecID (1B)
					// pBuf[1] == AVCPacketType (1B)
					// pBuf[2] == 0 (3B)
					// pBuf[5] == AVCDecoderConfigurationRecord
					const uint8_t *p = pbuf + 5;

					//	aligned(8) class AVCDecoderConfigurationRecord {
					//		unsigned int(8) configurationVersion = 1;
					//		unsigned int(8) AVCProfileIndication;
					//		unsigned int(8) profile_compatibility;
					//		unsigned int(8) AVCLevelIndication;
					//		bit(6) reserved = ‘111111’b;
					//		unsigned int(2) lengthSizeMinusOne;
					//		bit(3) reserved = ‘111’b;
					//		unsigned int(5) numOfSequenceParameterSets;
					//		for (i=0; i< numOfSequenceParameterSets;  i++) {
					//			unsigned int(16) sequenceParameterSetLength ;
					//			bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
					//		}
					//		unsigned int(8) numOfPictureParameterSets;
					//		for (i=0; i< numOfPictureParameterSets;  i++) {
					//			unsigned int(16) pictureParameterSetLength;
					//			bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
					//		}
					//		if( profile_idc  ==  100  ||  profile_idc  ==  110  ||
					//		    profile_idc  ==  122  ||  profile_idc  ==  144 )
					//		{
					//			bit(6) reserved = ‘111111’b;
					//			unsigned int(2) chroma_format;
					//			bit(5) reserved = ‘11111’b;
					//			unsigned int(3) bit_depth_luma_minus8;
					//			bit(5) reserved = ‘11111’b;
					//			unsigned int(3) bit_depth_chroma_minus8;
					//			unsigned int(8) numOfSequenceParameterSetExt;
					//			for (i=0; i< numOfSequenceParameterSetExt; i++) {
					//				unsigned int(16) sequenceParameterSetExtLength;
					//				bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
					//			}
					//		}
					//	}
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

					break;
				}

				case 1:
				{
					// AVC NALU

					// pBuf[0] == CodecID (1B)
					// pBuf[1] == AVCPacketType (1B)
					// pBuf[2] == CompositionTime offset (3B)
					// pBuf[5] == One or more NALUs
					//            (can be individual slices per FLV packets;
					//            that is, full frames are not strictly required)
                    cts += static_cast<int64_t>(static_cast<uint8_t>(*(pbuf + 2)) << 16 |
                                                static_cast<uint8_t>(*(pbuf + 3)) << 8 |
                                                static_cast<uint8_t>(*(pbuf + 4)));

                    const uint8_t *p = pbuf + 5;

					// 0      Unspecified                                                    non-VCL
					// 1      Coded slice of a non-IDR picture                               VCL
					// 2      Coded slice data partition A                                   VCL
					// 3      Coded slice data partition B                                   VCL
					// 4      Coded slice data partition C                                   VCL
					// 5      Coded slice of an IDR picture                                  VCL
					// 6      Supplemental enhancement information (SEI)                     non-VCL
					// 7      Sequence parameter set                                         non-VCL
					// 8      Picture parameter set                                          non-VCL
					// 9      Access unit delimiter                                          non-VCL
					// 10     End of sequence                                                non-VCL
					// 11     End of stream                                                  non-VCL
					// 12     Filler data                                                    non-VCL
					// 13     Sequence parameter set extension                               non-VCL
					// 14     Prefix NAL unit                                                non-VCL
					// 15     Subset sequence parameter set                                  non-VCL
					// 16     Depth parameter set                                            non-VCL
					// 17..18 Reserved                                                       non-VCL
					// 19     Coded slice of an auxiliary coded picture without partitioning non-VCL
					// 20     Coded slice extension                                          non-VCL
					// 21     Coded slice extension for depth view components                non-VCL
					// 22..23 Reserved                                                       non-VCL
					// 24..31 Unspecified                                                    non-VCL

					auto tmp_buf = std::make_shared<MediaFrame>();

					while(true)
					{
						int32_t nal_length = static_cast<int32_t>(static_cast<uint8_t>(*(p)) << 24 |
						                                          static_cast<uint8_t>(*(p + 1)) << 16 |
						                                          static_cast<uint8_t>(*(p + 2)) << 8 |
						                                          static_cast<uint8_t>(*(p + 3)));

						// skip length 32bit
						p += 4;

						tmp_buf->AppendBuffer(start_code, 4);
						tmp_buf->AppendBuffer(p, nal_length);

						p += nal_length;

						if((p - pbuf) >= pbuf_length)
						{
							break;
						}

						// logtd("nal_unit_type : %s", Nalu2Str( (AvcNaluType)((*p) & 0x1F) ).c_str());
					}

					data->Clear();
					data->Append(tmp_buf->GetBuffer(), tmp_buf->GetBufferSize());
					break;
				}

				case 2:
				{
					// AVC end of sequence (lower level NALU sequence ender is not required or supported)
					data->Clear();
					data->Append(start_code, 4);
					break;
				}

				default:
					break;
			}

			break;
		}

		case 2:
		{
            cts += static_cast<int64_t>(static_cast<uint8_t>(*(pbuf + 2)) << 16 |
                                        static_cast<uint8_t>(*(pbuf + 3)) << 8 |
                                        static_cast<uint8_t>(*(pbuf + 4)));
			// intra-frame
			const uint8_t *p = pbuf + 5;

			auto tmp_buf = std::make_shared<MediaFrame>();

			while(true)
			{
				int32_t nal_length = static_cast<int32_t>(static_cast<uint8_t>(*(p)) << 24 |
				                                          static_cast<uint8_t>(*(p + 1)) << 16 |
				                                          static_cast<uint8_t>(*(p + 2)) << 8 |
				                                          static_cast<uint8_t>(*(p + 3)));

				// logte("nal_lengh = %d, pbuf_length = %d", nal_length, pbuf_length);

				// skip length 32bit
				p += 4;

				// logtd("n
				// al_unit_type : %s", Nalu2Str( (AvcNaluType)((*p) & 0x1F) ).c_str());

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

			break;
		}

		case 3:
			// disposable inter frame (H.263 only)
			break;

		case 4:
			// generated keyframe (reserved for server use only)
			break;

		case 5:
			// video info/command frame
			break;

		default:
			break;
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
	avc_profile = *(data++);
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
