
#include "h264_avcc_to_annexb.h"
#include "h264_decoder_configuration_record.h"
#include "h264_sps.h"


#define OV_LOG_TAG "H264AvccToAnnexB"

static uint8_t START_CODE[4] = { 0x00, 0x00, 0x00, 0x01 };

bool H264AvccToAnnexB::GetExtradata(const common::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata)
{
    if(type == common::PacketType::SEQUENCE_HEADER)
    {
        AVCDecoderConfigurationRecord config;
        if(!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
        {
            logte("sequence heaer paring error"); 
            return false;
        }
        
        extradata = config.Serialize();

        logtd("%s\r\n%d", config.GetInfoString().CStr(), extradata.size());;

        return true;   
    }

    return false;
}

bool H264AvccToAnnexB::Convert(const std::shared_ptr<MediaPacket> &packet)
{
    common::BitstreamFormat bitstream_format = packet->GetBitstreamFormat();
    common::PacketType pkt_type = packet->GetPacketType();

    if(bitstream_format == common::BitstreamFormat::H264_ANNEXB)
    {
        return true;
    }

    if(H264AvccToAnnexB::Convert(pkt_type, packet->GetData()) == false)
    {
        return false;
    }

    packet->SetBitstreamFormat(common::BitstreamFormat::H264_ANNEXB);
    packet->SetPacketType(common::PacketType::NALU);

    return true;
}

bool H264AvccToAnnexB::Convert(common::PacketType type, const std::shared_ptr<ov::Data> &data)
{
    auto annexb_data = std::make_shared<ov::Data>();

    annexb_data->Clear();
    if(type == common::PacketType::SEQUENCE_HEADER)
    {
        AVCDecoderConfigurationRecord config;
        if(!AVCDecoderConfigurationRecord::Parse(data->GetDataAs<uint8_t>(), data->GetLength(), config))
        {
            logte("sequence heaer paring error"); 
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
    data->Append(annexb_data);

    return true;
}
