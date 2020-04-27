//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "bitstream_to_annexb.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "BitstreamToAnnexB"

static uint8_t START_CODE[4] = { 0x00, 0x00, 0x00, 0x01 };

bool BitstreamToAnnexB::IsAnnexB(const uint8_t *buffer) const
{
	return
		(buffer[0] == 0x00) &&
		(buffer[1] == 0x00) &&
		((buffer[2] == 0x01) || (buffer[2] == 0x00 && buffer[3] == 0x01));
}

bool BitstreamToAnnexB::ConvertKeyFrame(AvcPacketType packet_type, ov::ByteStream read_stream, const std::shared_ptr<ov::Data> &data)
{
	switch(packet_type)
	{
		case AvcPacketType::SeqHeader:
		{
			// read_stream has the AVCDecoderConfigurationRecord

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

			if(read_stream.IsRemained(7) == false)
			{
				logte("Not enough data to parse AVCDecoderConfigurationRecord");
				return false;
			}

			[[maybe_unused]] auto configurationVersion = read_stream.Read8();
			logtd("configurationVersion = %d", configurationVersion);
			[[maybe_unused]] uint8_t AVCProfileIndication = read_stream.Read8();
			logtd("AVCProfileIndication = %d", AVCProfileIndication);
			[[maybe_unused]] uint8_t profile_compatibility = read_stream.Read8();
			logtd("profile_compatibility = %d", profile_compatibility);
			[[maybe_unused]] uint8_t AVCLevelIndication = read_stream.Read8();
			logtd("AVCLevelIndication = %d", AVCLevelIndication);
			[[maybe_unused]] uint8_t lengthSizeMinusOne = read_stream.Read8() & 0x03;
			logtd("lengthSizeMinusOne = %d", lengthSizeMinusOne);

			// Number of SPS
			uint8_t sps_byte = read_stream.Read8();
			int numOfSequenceParameterSets = sps_byte & 0x1Fu;

			// reserve(3) + unsigned int(5)
			logtd("numOfSequenceParameterSets = reserve(%02x) + %d", sps_byte & 0xE0, numOfSequenceParameterSets);

			for(int i = 0; i < numOfSequenceParameterSets; i++)
			{
				if(read_stream.IsRemained(2) == false)
				{
					logte("Not enough data to parse SPS");
					return false;
				}

				int sequenceParameterSetLength = read_stream.ReadBE16();

				if(read_stream.IsRemained(static_cast<size_t>(sequenceParameterSetLength)) == false)
				{
					logte("SPS data length (%d) is greater than buffer length (%d)", sequenceParameterSetLength, read_stream.Remained());
					return false;
				}

				logtd(" sequenceParameterSetLength = %d", sequenceParameterSetLength);

				_sps = read_stream.GetRemainData()->Subdata(0LL, static_cast<size_t>(sequenceParameterSetLength));
				read_stream.Skip(static_cast<size_t>(sequenceParameterSetLength));
			}

			// Number of PPS
			uint8_t pps_byte = read_stream.Read8();
			int numOfPictureParameterSets = pps_byte & 0x1Fu;

			// reserve(3) + unsigned int(5)
			logtd("numOfPictureParameterSets = %d", numOfPictureParameterSets);

			for(int i = 0; i < numOfPictureParameterSets; i++)
			{
				if(read_stream.IsRemained(2) == false)
				{
					logte("Not enough data to parse PPS");
					return false;
				}

				int pictureParameterSetLength = read_stream.ReadBE16();

				if(read_stream.IsRemained(static_cast<size_t>(pictureParameterSetLength)) == false)
				{
					logte("PPS data length (%d) is greater than buffer length (%d)", pictureParameterSetLength, read_stream.Remained());
					return false;
				}

				logtd(" pictureParameterSetLength = %d", pictureParameterSetLength);

				_pps = read_stream.GetRemainData()->Subdata(0LL, static_cast<size_t>(pictureParameterSetLength));
				read_stream.Skip(static_cast<size_t>(pictureParameterSetLength));
			}

			data->Clear();

			data->Append(START_CODE, sizeof(START_CODE));
			data->Append(_sps.get());

			data->Append(START_CODE, sizeof(START_CODE));
			data->Append(_pps.get());

			return true;
		}

		case AvcPacketType::Nalu:
		{
			data->Clear();

			// Important! 
			//  주기적으로 SPS/PPS를 전달하기위한 목적으로 사용된다. 만약, 주기적으로 SPS, PPS가 전달되지 않으면 플레이어에서 재생이 안된다. 
			//  x264 SW 코덱은 Key Frame에 IDR NAL 패킷만 포함되어 있어서 아래와 같이 SPS, PPS 패킷을 추가해줘야 한다.
			// TODO(soulk) : NVENC HW 코덱은 Key Frame 에 SPS+PPS+IDR NAL 패킷이 모두 포함되어 있다. 
			//				 중복으로 SPS/PPS 전송되지 않도록 예외 처리해야 한다.

			data->Append(START_CODE, sizeof(START_CODE));
			data->Append(_sps.get());

			data->Append(START_CODE, sizeof(START_CODE));
			data->Append(_pps.get());

			return ConvertInterFrame(packet_type, read_stream, data);
		}

		case AvcPacketType::EndOfSeq:
		{
			// AVC end of sequence (lower level NALU sequence ender is not required or supported)
			data->Clear();

			data->Append(START_CODE, sizeof(START_CODE));

			return true;
		}
	}

	return false;
}

bool BitstreamToAnnexB::ConvertInterFrame(AvcPacketType packet_type, ov::ByteStream read_stream, const std::shared_ptr<ov::Data> &data)
{
	// data->Clear();

	while(read_stream.Remained() > 0)
	{
		if(read_stream.IsRemained(4) == false)
		{
			logte("Not enough data to parse NAL");
			return false;
		}

		size_t nal_length = read_stream.ReadBE32();

		if(read_stream.IsRemained(nal_length) == false)
		{
			logte("NAL length (%d) is greater than buffer length (%d)", nal_length, read_stream.Remained());
			return false;
		}

		auto nal_data = read_stream.GetRemainData()->Subdata(0LL, nal_length);

		[[maybe_unused]] auto skipped = read_stream.Skip(nal_length);
		OV_ASSERT2(skipped == nal_length);

		data->Append(START_CODE, sizeof(START_CODE));
		data->Append(nal_data.get());
	}

	return true;
}

bool BitstreamToAnnexB::Convert(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	auto data_for_read = data->Clone();
	ov::ByteStream read_stream(data_for_read.get());
	uint8_t video_data[4];

	// At least 4 bytes are required to determine bit stream type
	if(read_stream.Read(video_data, OV_COUNTOF(video_data)) != OV_COUNTOF(video_data))
	{
		logtw("Could not determine bit stream type");
		return false;
	}

	// Check if the bitstream is Annex-B type
	if(IsAnnexB(video_data))
	{
		// Do not need to convert bit stream because the stream is already annexb type
		return true;
	}

	uint8_t last_composition_byte = 0;
	if(read_stream.Read(&last_composition_byte) == 0)
	{
		logtw("Could not obtain composition time");
		return false;
	}

	//
	// Convert [FLV] to [Video data]
	//
	// Refrence: Video File Format Specification, Version 10 (https://wwwimages2.adobe.com/content/dam/acom/en/devnet/flv/video_file_format_spec_v10.pdf)

	// [Video data]
	//   4bit - frame_type
	auto frame_type = static_cast<AvcFrameType>(static_cast<uint8_t>(video_data[0] >> 4u) & 0x0Fu);
	//   4bit - codec_id
	auto codec_id = static_cast<AvcCodecId>(video_data[0] & 0x0Fu);
	if(codec_id != AvcCodecId::AVC)
	{
		logte("Not supported codec: %d", codec_id);
		return false;
	}
	//   8bit : AVCPacketType
	auto packet_type = static_cast<AvcPacketType>(video_data[1]);
	//   24bit - composition time
	if(packet_type == AvcPacketType::Nalu)
	{
		cts =
			(static_cast<uint32_t>(video_data[2]) << 16u) |
			(static_cast<uint32_t>(video_data[3]) << 8u) |
			static_cast<uint32_t>(last_composition_byte);
	}
	else
	{
		cts = 0LL;
	}

	switch(frame_type)
	{
		case AvcFrameType::Key:
			return ConvertKeyFrame(packet_type, read_stream, data);

		case AvcFrameType::Inter:
			data->Clear();
			return ConvertInterFrame(packet_type, read_stream, data);

		case AvcFrameType::DisposableInter:
		case AvcFrameType::GeneratedKey:
		case AvcFrameType::VideoInfo:
			break;
	}
	return false;
}

//====================================================================================================
// SequenceHeaderParsing
// - Bitstream(Rtmp Input Low Data) Sequence Info Parsing
//====================================================================================================
bool BitstreamToAnnexB::ParseSequenceHeader(const uint8_t *data,
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
	[[maybe_unused]] auto version = *(data++);
	logtd("configurationVersion  = %d", version);
	avc_profile = *(data++);
	avc_profile_compatibility = *(data++);
	avc_level = *(data++);
	[[maybe_unused]] auto length_size = *(data++) & 0x03;
	logtd("lengthSizeMinusOne = %d", length_size);

	logtd("numOfSequenceParameterSets = reserve(%02x) + %d", *data & 0xE0, *data & 0x1F); // reserve(3) + unsigned int(5)
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

		logtd(" sequenceParameterSetLength = %d", sps_size);
	}


	logtd("numOfPictureParameterSets = %d", *data); // reserve(3) + unsigned int(5)
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

		logtd(" pictureParameterSetLength = %d", pps_size);
	}

	return true;
}
