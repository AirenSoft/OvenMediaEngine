#pragma once

#include <cstdint>
#include <vector>

// Parses the payload of the NAL unit without the starting byte
class H264NalUnitBitstreamParser
{
public:
	H264NalUnitBitstreamParser(const uint8_t *bitstream, size_t length);

	bool ReadBit(uint8_t &value);
	bool ReadU8(uint8_t &value);
	bool ReadU16(uint16_t &value);
	bool ReadU32(uint32_t &value);
	bool ReadUEV(uint32_t &value);
	bool ReadSEV(int32_t &value);
	bool Skip(uint32_t count);

private:
	std::vector<uint8_t> bitstream_;
	size_t total_bits_;
	size_t bit_offset_ = 0;
};