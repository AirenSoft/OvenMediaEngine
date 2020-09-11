
#include "h264_avcc_to_annexb.h"
#include "h264_decoder_configuration_record.h"
#include "h264_parser.h"


#define OV_LOG_TAG "H264AvccToAnnexB"

static uint8_t START_CODE[4] = { 0x00, 0x00, 0x00, 0x01 };

bool H264AvccToAnnexB::GetExtradata(const common::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata)
{
	if(type == common::PacketType::SEQUENCE_HEADER)
	{
		AVCDecoderConfigurationRecord config;
		if(!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
		{
			logte("Could not parse sequence header"); 
			return false;
		}

		// Structure of Extradata
		//  START_CODE + SPS + START_CODE + PPS ... 
		auto tmp = std::make_shared<ov::Data>();

		for(int i=0 ; i<config.NumOfSPS() ; i++)
		{
			tmp->Append(START_CODE, sizeof(START_CODE));    
			tmp->Append(config.GetSPS(i));
		}
		for(int i=0 ; i<config.NumOfPPS() ; i++)
		{
			tmp->Append(START_CODE, sizeof(START_CODE));    
			tmp->Append(config.GetPPS(i));
		}

		extradata.reserve(tmp->GetLength());
		std::copy(tmp->GetDataAs<uint8_t>(), tmp->GetDataAs<uint8_t>()+tmp->GetLength(), std::back_inserter(extradata));

		return true;   
	}

	return false;
}

bool H264AvccToAnnexB::Convert(common::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata)
{
	auto annexb_data = std::make_shared<ov::Data>();

	annexb_data->Clear();

	if(type == common::PacketType::SEQUENCE_HEADER)
	{
		AVCDecoderConfigurationRecord config;
		if(!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
		{
			logte("Could not parse sequence header"); 
			return false;
		}
		logtd("%s", config.GetInfoString().CStr());

		for(int i=0 ; i<config.NumOfSPS() ; i++)
		{
			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(config.GetSPS(i));
		}
		
		for(int i=0 ; i<config.NumOfPPS() ; i++)
		{
			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(config.GetPPS(i));
		}
	}
	else if(type == common::PacketType::NALU)
	{
		ov::ByteStream read_stream(data.get());

		// Append SPS/PPS Nalunit
		if(extradata.size() > 0)
		{
			annexb_data->Append(extradata.data(), (size_t)extradata.size());
		}

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

			annexb_data->Append(START_CODE, sizeof(START_CODE));
			annexb_data->Append(nal_data);
		}               
	}

	data->Clear();
	
	if(annexb_data->GetLength() > 0)
	{
		data->Append(annexb_data);
	}

	return true;
}
