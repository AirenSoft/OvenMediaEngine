
#include "h264_converter.h"

#include "h264_decoder_configuration_record.h"
#include "h264_parser.h"

#define OV_LOG_TAG "H264Converter"

static uint8_t START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

bool H264Converter::GetExtraDataFromAvcc(const cmn::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata)
{
	if (type == cmn::PacketType::SEQUENCE_HEADER)
	{
		AVCDecoderConfigurationRecord config;
		if (!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
		{
			logte("Could not parse sequence header");
			return false;
		}
		logtd("%s", config.GetInfoString().CStr());

		config.Serialize(extradata);

		return true;
	}

	return false;
}

bool H264Converter::ConvertAvccToAnnexb(cmn::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata)
{
	auto annexb_data = std::make_shared<ov::Data>();

	if (type == cmn::PacketType::SEQUENCE_HEADER)
	{
		AVCDecoderConfigurationRecord config;
		if (!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
		{
			logte("Could not parse sequence header");
			return false;
		}

		for (int i = 0; i < config.NumOfSPS(); i++)
		{
			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(config.GetSPS(i));
		}

		for (int i = 0; i < config.NumOfPPS(); i++)
		{
			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(config.GetPPS(i));
		}
	}
	else if (type == cmn::PacketType::NALU)
	{
		ov::ByteStream read_stream(data.get());

		bool has_idr_slice = false;

		while (read_stream.Remained() > 0)
		{
			if (read_stream.IsRemained(4) == false)
			{
				logte("Not enough data to parse NAL");
				return false;
			}

			size_t nal_length = read_stream.ReadBE32();

			if (read_stream.IsRemained(nal_length) == false)
			{
				logte("NAL length (%d) is greater than buffer length (%d)", nal_length, read_stream.Remained());
				return false;
			}

			auto nal_data = read_stream.GetRemainData()->Subdata(0LL, nal_length);
			[[maybe_unused]] auto skipped = read_stream.Skip(nal_length);
			OV_ASSERT2(skipped == nal_length);

			H264NalUnitHeader header;
			if (H264Parser::ParseNalUnitHeader(nal_data->GetDataAs<uint8_t>(), H264_NAL_UNIT_HEADER_SIZE, header) == true)
			{
				if (header.GetNalUnitType() == H264NalUnitType::IdrSlice)
					has_idr_slice = true;
			}

			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(nal_data);
		}

		// Deprecated. The same function is performed in Mediarouter.

		// Append SPS/PPS NalU before IdrSlice NalU. not every packet.
		if (extradata.size() > 0 && has_idr_slice == true)
		{
			AVCDecoderConfigurationRecord config;
			if (!AVCDecoderConfigurationRecord::Parse(extradata.data(), extradata.size(), config))
			{
				logte("Could not parse sequence header");
				return false;
			}

			auto sps_pps = std::make_shared<ov::Data>();

			for (int i = 0; i < config.NumOfSPS(); i++)
			{
				sps_pps->Append(START_CODE, sizeof(START_CODE));
				sps_pps->Append(config.GetSPS(i));
			}

			for (int i = 0; i < config.NumOfPPS(); i++)
			{
				sps_pps->Append(START_CODE, sizeof(START_CODE));
				sps_pps->Append(config.GetPPS(i));
			}

			annexb_data->Insert(sps_pps->GetDataAs<uint8_t>(), 0, sps_pps->GetLength());
		}
	}

	data->Clear();

	if (annexb_data->GetLength() > 0)
	{
		data->Append(annexb_data);
		//  *data = *annexb_data;
	}

	return true;
}

static inline int GetStartPatternSize(const void *data, size_t length, int first_pattern_size)
{
	if (data != nullptr)
	{
		auto buffer = static_cast<const uint8_t *>(data);

		if (first_pattern_size > 0)
		{
			if ((buffer[0] == 0x00) && (buffer[1] == 0x00))
			{
				if (
					(first_pattern_size == 4) &&
					((length >= 4) && (buffer[2] == 0x00) && (buffer[3] == 0x01)))
				{
					// 0x00 0x00 0x00 0x01 pattern
					return 4;
				}
				else if (
					(first_pattern_size == 3) &&
					((length >= 3) && (buffer[2] == 0x01)))
				{
					// 0x00 0x00 0x01 pattern
					return 3;
				}
			}

			// pattern_size must be the same with the first_pattern_size
			return -1;
		}
		else
		{
			// probe mode

			if ((length >= 4) && ((buffer[0] == 0x00) && (buffer[1] == 0x00) && (buffer[2] == 0x00) && (buffer[3] == 0x01)))
			{
				// 0x00 0x00 0x00 0x01 pattern
				return 4;
			}
			if ((length >= 3) && ((buffer[0] == 0x00) && (buffer[1] == 0x00) && (buffer[2] == 0x01)))
			{
				// 0x00 0x00 0x01 pattern
				return 3;
			}
		}
	}

	return -1;
}

#if 0
static bool ExtractSpsPpsOffset(const std::shared_ptr<const ov::Data> &data, const std::vector<size_t> &offset_list, const std::vector<size_t> &pattern_size_list,
								const std::shared_ptr<ov::Data> &sps, const std::shared_ptr<ov::Data> &pps)
{
	auto buffer = data->GetDataAs<uint8_t>();
	size_t offset_count = offset_list.size();

	// offset_list contains the length of data at the end
	for (size_t index = 0; index < (offset_count - 1); index++)
	{
		size_t nalu_offset = offset_list[index] + pattern_size_list[index];
		size_t nalu_data_len = offset_list[index + 1] - nalu_offset;

		// NAL unit type codes
		//
		// nal_unit_type	Content of NAL unit and RBSP syntax structure		C
		// ---------------------------------------------------------------------------------
		// 0				Unspecified
		// 1				Coded slice of a non-IDR picture					2, 3, 4
		// 					- slice_layer_without_partitioning_rbsp()
		// 2				Coded slice data partition A						2
		// 					- slice_data_partition_a_layer_rbsp()
		// 3				Coded slice data partition B						3
		// 					- slice_data_partition_b_layer_rbsp()
		// 4				Coded slice data partition C						4
		// 					- slice_data_partition_c_layer_rbsp()
		// 5				Coded slice of an IDR picture						2, 3
		// 					- slice_layer_without_partitioning_rbsp()
		// 6				Supplemental enhancement information (SEI)			5
		// 					- sei_rbsp()
		// 7				Sequence parameter set								0
		// 					- seq_parameter_set_rbsp()
		// 8				Picture parameter set								1
		// 					- pic_parameter_set_rbsp()
		// 9				Access unit delimiter								6
		// 					- access_unit_delimiter_rbsp()
		// 10				End of sequence										7
		// 					- end_of_seq_rbsp()
		// 11				End of stream										8
		// 					- end_of_stream_rbsp()
		// 12				Filler data											9
		// 					- filler_data_rbsp()
		// 13				Sequence parameter set extension					10
		// 					- seq_parameter_set_extension_rbsp()
		// 14..18			Reserved
		// 19				Coded slice of an auxiliary coded picture			2, 3, 4
		// 					without partitioning
		// 					- slice_layer_without_partitioning_rbsp()
		// 20..23			Reserved
		// 24..31			Unspecified
		uint8_t nalu_header = *(buffer + nalu_offset);
		uint8_t nal_unit_type = nalu_header & 0x01F;

		switch (nal_unit_type)
		{
			case 7:
				// SPS
				sps->Clear();
				sps->Append(buffer + nalu_offset, nalu_data_len);
				break;

			case 8:
				// PPS
				pps->Clear();
				pps->Append(buffer + nalu_offset, nalu_data_len);
				break;
		}
	}

	if (sps->IsEmpty() || pps->IsEmpty())
	{
		logte("Could not parse SPS/PPS (SPS: %zu, PPS: %zu)", sps->GetLength(), pps->GetLength());
		return false;
	}

	return true;
}
#endif

std::shared_ptr<const ov::Data> H264Converter::ConvertAnnexbToAvcc(const std::shared_ptr<const ov::Data> &data)
{
	// size_t total_pattern_length = 0;

	auto buffer = data->GetDataAs<uint8_t>();
	size_t remained = data->GetLength();
	off_t offset = 0;
	off_t last_offset = 0;

	auto avcc_data = std::make_shared<ov::Data>();
	ov::ByteStream byte_stream(avcc_data);

	int first_pattern_size = 0;

	// This code assumes that (NALULengthSizeMinusOne == 3)
	while (remained > 0)
	{
		if (*buffer == 0x00)
		{
			auto pattern_size = GetStartPatternSize(buffer, remained, first_pattern_size);

			if (pattern_size > 0)
			{
				if (last_offset < offset)
				{
					auto nalu = data->Subdata(last_offset, offset - last_offset);

					byte_stream.WriteBE32(nalu->GetLength());
					byte_stream.Write(nalu);

					last_offset = offset;
				}

				buffer += pattern_size;
				offset += pattern_size;
				last_offset += pattern_size;
				remained -= pattern_size;

				continue;
			}
		}

		buffer++;
		offset++;
		remained--;
	}

	if (last_offset < offset)
	{
		// Append remained data
		auto nalu = data->Subdata(last_offset, offset - last_offset);

		byte_stream.WriteBE32(nalu->GetLength());
		byte_stream.Write(nalu);

		last_offset = offset;
	}

	return avcc_data;
}

ov::String H264Converter::GetProfileString(const std::vector<uint8_t> &codec_extradata)
{
	AVCDecoderConfigurationRecord record;

	if (AVCDecoderConfigurationRecord::Parse(codec_extradata, record) == false)
	{
		return "";
	}

	// PPCCLL = <profile idc><constraint set flags><level idc>
	return ov::String::FormatString(
		"%02x%02x%02x",
		record.ProfileIndication(), record.Compatibility(), record.LevelIndication());
}
