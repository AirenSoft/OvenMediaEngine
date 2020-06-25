#include "bitstream_conv.h"


BitstreamConv::BitstreamConv() :
	_type(ConvUnknown)
{

}

BitstreamConv::~BitstreamConv()
{

}

void BitstreamConv::SetType(ConvType type)
{
	_type = type;
}

BitstreamConv::ResultType BitstreamConv::Convert(const std::shared_ptr<ov::Data> &data)
{
	int64_t cts = 0;

	return Convert(_type, data, cts);
}

BitstreamConv::ResultType BitstreamConv::Convert(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	return Convert(_type, data, cts);
}

BitstreamConv::ResultType BitstreamConv::Convert(ConvType type, const std::shared_ptr<ov::Data> &data)
{
	int64_t cts = 0;

	return Convert(type, data, cts);
}

BitstreamConv::ResultType BitstreamConv::Convert(ConvType type, const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	switch(type)
	{
		case ConvADTS:
		{
			int32_t data_len = _adts.Convert(data);
			return (data_len>0)?SUCCESS_DATA:(data_len==0)?SUCCESS_NODATA:INVALID_DATA;
		}
		break;
		case ConvAnnexA:
		{
			int32_t data_len = _annexa.Convert(data);
			return (data_len>0)?SUCCESS_DATA:(data_len==0)?SUCCESS_NODATA:INVALID_DATA;
		} 
		break;
		case ConvAnnexB:
		{
			bool ret = _annexb.Convert(data, cts);
			return (ret==true)?SUCCESS_DATA:SUCCESS_NODATA;
		}
		break;
		case ConvUnknown:
		default:
			break;
	}

	return ResultType::INVALID_DATA;	
}

bool BitstreamConv::ParseSequenceHeaderAVC(const uint8_t *data,
	int data_size,
	std::vector<uint8_t> &_sps,
	std::vector<uint8_t> &_pps,
	uint8_t &avc_profile,
	uint8_t &avc_profile_compatibility,
	uint8_t &avc_level)
{
	return BitstreamToAnnexB::ParseSequenceHeader(data, data_size, _sps, _pps, avc_profile, avc_profile_compatibility, avc_level);
}

bool BitstreamConv::MakeAVCFragmentHeader(const std::shared_ptr<MediaPacket> &packet)
{
   auto fragment_header = packet->GetFragHeader();

    const uint8_t* srcData = static_cast<const uint8_t*>(packet->GetData()->GetData());
    size_t dataOffset = 0;
    size_t dataSize = packet->GetData()->GetLength();

    std::vector<std::pair<size_t, size_t>> offset_list;

    // Search for START_CODE in the NAL header.
    // Make a list of offset for NAL packets except START_CODE
    while ( dataOffset < dataSize )
    {
        size_t remainDataSize = dataSize - dataOffset;
        const uint8_t* data = srcData + dataOffset;

        if (remainDataSize >= 3 &&
                0x00 == data[0] &&
                0x00 == data[1] &&
                0x01 == data[2])
        {
            offset_list.emplace_back(dataOffset, 3); // Offset, SIZEOF(START_CODE[3])
            dataOffset += 3;
        }
        else if (remainDataSize >= 4 &&
                 0x00 == data[0] &&
                 0x00 == data[1] &&
                 0x00 == data[2] &&
                 0x01 == data[3])
        {
            offset_list.emplace_back(dataOffset, 4); // Offset, SIZEOF(START_CODE[4])
            dataOffset += 4;
        }
        else
        {
            dataOffset += 1;
        }
    }

    fragment_header->Clear();


    for (size_t index = 0; index < offset_list.size(); ++index)
    {
        size_t nalu_offset = 0;
        size_t nalu_data_len = 0;

        if (index != offset_list.size() - 1)
        {
            nalu_offset = offset_list[index].first + offset_list[index].second;
            nalu_data_len = offset_list[index + 1].first - nalu_offset;
        }
        else
        {
            nalu_offset = offset_list[index].first + offset_list[index].second;
            nalu_data_len = dataSize - nalu_offset;
        }

 // for debug
#if 0  
        uint8_t nalu_header = *(srcData + nalu_offset);
        uint8_t forbidden_zero_bit = (nalu_header >> 7)  & 0x01;
        uint8_t nal_ref_idc = (nalu_header >> 5)  & 0x03;
        uint8_t nal_unit_type = (nalu_header)  & 0x01F;

        // if( (nal_unit_type == (uint8_t)BitstreamToAnnexB::AvcNaluType::IDR) 
            || (nal_unit_type == (uint8_t))BitstreamToAnnexB::AvcNaluType::SEI) 
            || (nal_unit_type == (uint8_t))BitstreamToAnnexB::AvcNaluType::SPS) 
            || (nal_unit_type == (uint8_t))BitstreamToAnnexB::AvcNaluType::PPS))
        {
            logtd("[%d][%d] nal_ref_idc:%2d, nal_unit_type:%2d => offset:%d, nalu_size:%d, nalu_offset:%d, nalu_length:%d"
            , index
            , dataSize
            , nal_ref_idc, nal_unit_type
            , offset_list[index].first
            , offset_list[index].second
            , nalu_offset
            , nalu_data_len
                 );
        }

        // if (nal_unit_type == (uint8_t))BitstreamToAnnexB::AvcNaluType::SEI) // SEI
        //     logtd("%s", ov::Dump(srcData+nalu_offset, nalu_data_len, 32).CStr());        
#endif


        fragment_header->fragmentation_offset.emplace_back(nalu_offset);
        fragment_header->fragmentation_length.emplace_back(nalu_data_len);
    }

    return true;
}

