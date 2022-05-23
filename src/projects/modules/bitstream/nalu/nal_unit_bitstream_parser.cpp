#include "nal_unit_bitstream_parser.h"

NalUnitBitstreamParser::NalUnitBitstreamParser(const uint8_t *bitstream, size_t length)
	: BitReader(nullptr, 0)
{
    // Parse the bitstream and skip emulation_prevention_three_byte
   	_bitstream.reserve(length);

    for (size_t original_bitstream_offset = 0; original_bitstream_offset < length;)
    {
		// 00 00 03 00 ==> 00 00 00
		// 00 00 03 01 ==> 00 00 01
		// 00 00 03 02 ==> 00 00 02
		// 00 00 03 03 ==> 00 00 03

        if (original_bitstream_offset + 3 < length && 
					bitstream[original_bitstream_offset] == 0x00 && 
					bitstream[original_bitstream_offset + 1] == 0x00 && 
					bitstream[original_bitstream_offset + 2] == 0x03 &&
					(bitstream[original_bitstream_offset + 3] & 0xFC) == 0) // 0b00 (0) || 0b01 (1) || 0b10 (2) || 0b11 (3)
        {
            _bitstream.emplace_back(bitstream[original_bitstream_offset]);
			original_bitstream_offset ++;
            _bitstream.emplace_back(bitstream[original_bitstream_offset]);
			original_bitstream_offset += 2; // skip the '03'
			_bitstream.emplace_back(bitstream[original_bitstream_offset]);
			original_bitstream_offset ++;
        }
        else
        {
            _bitstream.emplace_back(bitstream[original_bitstream_offset]);
			original_bitstream_offset ++;
        }
    }
    
	_buffer = _bitstream.data();
	_capacity = _bitstream.size();
	_position = _buffer;
}

bool NalUnitBitstreamParser::ReadU8(uint8_t &value)
{
    return ReadBits(8, value);
}

bool NalUnitBitstreamParser::ReadU16(uint16_t &value)
{
    return ReadBits(16, value);
}

bool NalUnitBitstreamParser::ReadU32(uint32_t &value)
{
	return ReadBits(32, value);
}

bool NalUnitBitstreamParser::ReadUEV(uint32_t &value)
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

bool NalUnitBitstreamParser::ReadSEV(int32_t &value)
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

bool NalUnitBitstreamParser::Skip(uint32_t count)
{
    uint64_t dummy;
	return ReadBits(count, dummy);
}