#include <base/ovlibrary/byte_io.h>
#include "rtp_depacketizer_h264.h"

std::shared_ptr<ov::Data> RtpDepacketizerH264::ParseAndAssembleFrame(std::vector<std::shared_ptr<ov::Data>> payload_list)
{
	if(payload_list.size() <= 0)
	{
		return nullptr;
	}

	auto bitstream = std::make_shared<ov::Data>();

	for(const auto &payload : payload_list)
	{
		uint8_t nal_type = (payload->GetDataAs<uint8_t>()[0]) & NAL_TYPE_MASK;
		std::shared_ptr<ov::Data> result;

		// Fragmented NAL units
		if(nal_type == NaluType::kFuA)
		{
			result = ParseFuaAndConvertAnnexB(payload);
			if(result == nullptr)
			{
				return nullptr;
			}

			bitstream->Append(result);
		}
		else if(nal_type == NaluType::kStapA)
		{
			result = ParseStapAAndConvertToAnnexB(payload);
			if(result == nullptr)
			{
				return nullptr;
			}

			bitstream->Append(result);
		}
		else
		{
			result = ConvertSingleNaluToAnnexB(payload);
			if(result == nullptr)
			{
				return nullptr;
			}

			bitstream->Append(result);
		}
	}

	return bitstream;
}

std::shared_ptr<ov::Data> RtpDepacketizerH264::ParseFuaAndConvertAnnexB(const std::shared_ptr<ov::Data> &payload)
{
	auto bitstream = std::make_shared<ov::Data>();

	if(payload->GetLength() < FUA_HEADER_SIZE)
	{
		// Invalid Data
		return nullptr;
	}

	auto buffer = payload->GetDataAs<uint8_t>();
	auto fnri = buffer[0] & (NAL_FBIT | NAL_NRI_MASK);
	auto original_nal_type = buffer[1] & NAL_TYPE_MASK;
	bool first_fragment = (buffer[1] & FUA_SBIT) > 0;

	if(first_fragment == true)
	{
		uint8_t	start_prefix_and_nal_header[ANNEXB_START_PREFIX_LENGTH + NAL_HEADER_SIZE];
		uint8_t original_nal_header = fnri | original_nal_type;

		logd("DEBUG", "FUA Nal Type : %d", original_nal_type);

		start_prefix_and_nal_header[0] = 0;
		start_prefix_and_nal_header[1] = 0;
		start_prefix_and_nal_header[2] = 0;
		start_prefix_and_nal_header[3] = 1;
		start_prefix_and_nal_header[4] = original_nal_header;

		bitstream->Append(start_prefix_and_nal_header, ANNEXB_START_PREFIX_LENGTH + NAL_HEADER_SIZE);
	}
	
	bitstream->Append(payload->Subdata(FUA_HEADER_SIZE));

	return bitstream;
}

std::shared_ptr<ov::Data> RtpDepacketizerH264::ParseStapAAndConvertToAnnexB(const std::shared_ptr<ov::Data> &payload)
{
	/*
	https://tools.ietf.org/html/rfc6184#section-5.7.1
	
	 0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          RTP Header                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         NALU 1 Data                           |
    :                                                               :
    +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |               | NALU 2 Size                   | NALU 2 HDR    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         NALU 2 Data                           |
    :                                                               :
    |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                               :...OPTIONAL RTP padding        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/

	auto bitstream = std::make_shared<ov::Data>();
	uint8_t start_prefix[ANNEXB_START_PREFIX_LENGTH] = {0, 0, 0, 1};

	if(payload->GetLength() < NAL_HEADER_SIZE + LENGTH_FIELD_SIZE)
	{
		return nullptr;
	}

	auto payload_buffer = payload->GetDataAs<uint8_t>();
	size_t payload_length = payload->GetLength();
	size_t offset = 0;

	offset += NAL_HEADER_SIZE;	// STAP-A NAL HDR

	while(offset < payload_length)
	{
		// Get NAL Length
		uint16_t nalu_size = ByteReader<uint16_t>::ReadBigEndian(&payload_buffer[offset]);
		offset += LENGTH_FIELD_SIZE;

		if(offset + nalu_size > payload_length)
		{
			return nullptr;
		}

		// Start Prefix
		bitstream->Append(start_prefix, ANNEXB_START_PREFIX_LENGTH);

		// Append NALU
		bitstream->Append(&payload_buffer[offset], nalu_size);

		uint8_t nal_type = payload_buffer[offset] & NAL_TYPE_MASK;

		logd("DEBUG", "STAP-A Nal Type : %d", nal_type);

		offset += nalu_size;
	}

	return bitstream;
}

std::shared_ptr<ov::Data> RtpDepacketizerH264::ConvertSingleNaluToAnnexB(const std::shared_ptr<ov::Data> &payload)
{
	auto bitstream = std::make_shared<ov::Data>(payload->GetLength() + ANNEXB_START_PREFIX_LENGTH);
	uint8_t start_prefix[ANNEXB_START_PREFIX_LENGTH] = {0, 0, 0, 1};

	bitstream->Append(start_prefix, ANNEXB_START_PREFIX_LENGTH);
	bitstream->Append(payload);

	uint8_t nal_type = payload->GetDataAs<uint8_t>()[0] & NAL_TYPE_MASK;
	logd("DEBUG", "Single Nal Type : %d", nal_type);

	return bitstream;
}