#include "nal_unit_splitter.h"

std::shared_ptr<NalUnitList> NalUnitSplitter::Parse(uint8_t* bitstream, size_t bitstream_length)
{
    auto nal_unit_list = std::make_shared<NalUnitList>();

    size_t  offset = 0;
    while(offset < bitstream_length)
    {
        size_t remaining = bitstream_length - offset;
        size_t start_pos = 0, end_pos = 0;
        uint8_t* data = bitstream + offset;

        if((remaining >= 3 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) || 
            (remaining >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01))
        {
            end_pos = offset - 1;

            if(data[2] == 0x01)
            {
                offset += 3;
            }
            else
            {
                offset += 4;
            }

            if(start_pos == 0)
            {
                start_pos = offset;
            }
            else
            {
                nal_unit_list->_nal_list.emplace_back(std::make_shared<ov::Data>(bitstream + start_pos, end_pos - start_pos));
                start_pos = 0;
            }
        }
        else
        {
            offset ++;
        }
        
        // last nal unit
        if(offset == bitstream_length && start_pos != 0)
        {
            end_pos = offset;
            nal_unit_list->_nal_list.emplace_back(std::make_shared<ov::Data>(bitstream + start_pos, end_pos - start_pos));
        }
    }
    
    return nal_unit_list;
}