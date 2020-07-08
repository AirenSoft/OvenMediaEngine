#include "h264_nal_unit_bitstream_parser.h"

H264NalUnitBitstreamParser::H264NalUnitBitstreamParser(const uint8_t *bitstream, size_t length)
{
    /*
        Parse the bitstream and skip emulation_prevention_three_byte instances along the way

        7.3.1 NAL unit syntax

        for( i = nalUnitHeaderBytes; i < NumBytesInNALunit; i++ ) {
            if( i + 2 < NumBytesInNALunit && next_bits( 24 ) = = 0x000003 ) {
                rbsp_byte[ NumBytesInRBSP++ ] All b(8)
                rbsp_byte[ NumBytesInRBSP++ ] All b(8)
                i += 2
                emulation_prevention_three_byte // equal to 0x03 All f(8)
            } else
                rbsp_byte[ NumBytesInRBSP++ ] All b(8)
        }
    */
    for (size_t original_bitstream_offset = 0; original_bitstream_offset < length;)
    {
        if (original_bitstream_offset < length + 2 && bitstream[original_bitstream_offset] == 0x00 && bitstream[original_bitstream_offset + 1] == 0x00 && bitstream[original_bitstream_offset + 2] == 0x03)
        {
            bitstream_.emplace_back(bitstream[original_bitstream_offset++]);
            bitstream_.emplace_back(bitstream[original_bitstream_offset++]);
            original_bitstream_offset++;
        }
        else
        {
            bitstream_.emplace_back(bitstream[original_bitstream_offset++]);
        }
    }
    total_bits_ = bitstream_.size() * 8;
}

bool H264NalUnitBitstreamParser::ReadBit(uint8_t &value)
{
    if (bit_offset_ >= total_bits_)
    {
        return false;
    }
    value = bitstream_[bit_offset_ / 8] & (0x80 >> (bit_offset_ % 8)) ? 1 : 0;
    bit_offset_ += 1;
    return true;
}

bool H264NalUnitBitstreamParser::ReadU8(uint8_t &value)
{
    if(bit_offset_ + 7 >= total_bits_)
    {
        return false;
    }
    size_t first_byte = bit_offset_ / 8;
    unsigned char bits_from_first_byte = 8 - bit_offset_ % 8;
    value = 0;
    if (bits_from_first_byte == 8)
    {
        value = bitstream_[first_byte];
    }
    else
    {
        unsigned char bits_from_second_byte = 8 - bits_from_first_byte;
        unsigned char first_bit_mask = 2 * bits_from_first_byte - 1;
        value |= (bitstream_[first_byte] & first_bit_mask) << (8 - bits_from_first_byte);
        value |= (bitstream_[first_byte + 1] & ~first_bit_mask) >> (8 - bits_from_second_byte);
    }
    bit_offset_ += 8;
    return true;
}

bool H264NalUnitBitstreamParser::ReadU16(uint16_t &value)
{
    uint8_t high_byte, low_byte;
    if (ReadU8(high_byte) && ReadU8(low_byte))
    {
        value = static_cast<uint16_t>(high_byte) << 8 | low_byte;
        return true;
    }
    return false;
}

bool H264NalUnitBitstreamParser::ReadU32(uint32_t &value)
{
    uint16_t high_part, low_part;
    if (ReadU16(high_part) && ReadU16(low_part))
    {
        value = static_cast<uint32_t>(high_part) << 16 | low_part;
        return true;
    }
    return false;
}

bool H264NalUnitBitstreamParser::ReadUEV(uint32_t &value)
{
    int zero_bit_count = 0;
    uint8_t bit;
    while (true)
    {
        if (ReadBit(bit) == false)
        {
            return false;
        }
        if (bit == 0)
        {
            zero_bit_count++;
        }
        else
        {
            break;
        }
    }
    value = 1;
    while (zero_bit_count--)
    {
        value <<= 1;
        if (ReadBit(bit) == false)
        {
            return false;
        }
        if (bit)
        {
            value += 1;
        }
    }
    value -= 1;
    return true;
}

bool H264NalUnitBitstreamParser::ReadSEV(int32_t &value)
{
    uint32_t uev_value;
    if (ReadUEV(uev_value) == false)
    {
        return false;
    }
    if (uev_value % 2 == 0)
    {
        uev_value = (uev_value + 1) / 2;
    }
    else
    {
        uev_value /= -2;
    }
    return true;
}

bool H264NalUnitBitstreamParser::Skip(uint32_t count)
{
    if (bit_offset_ + count >= total_bits_)
    {
        return false;
    }
    bit_offset_ += count;
    return true;
}