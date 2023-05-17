//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "bit_writer.h"

namespace ov
{
	BitWriter::BitWriter(uint32_t data_size)
	{
		_data = std::make_shared<std::vector<uint8_t>>(data_size, 0);
		_bit_count = 0;
	}

	bool BitWriter::Write(uint32_t bit_count, uint64_t value)
	{
		uint8_t *data = nullptr;
		uint32_t space = 0;

		data = _data->data();

		if (_bit_count + bit_count > _data->size() * 8)
		{
			return false;
		}

		data += _bit_count / 8;

		space = 8 - (_bit_count % 8);

		while (bit_count)
		{
			uint64_t mask = bit_count == 64 ? 0xFFFFFFFFFFFFFFFF : ((1ULL << bit_count) - 1);

			if (bit_count <= space)
			{
				*data |= ((value & mask) << (space - bit_count));
				_bit_count += bit_count;
				return true;
			}
			else
			{
				*data |= ((value & mask) >> (bit_count - space));
				++data;
				_bit_count += space;
				bit_count -= space;
				space = 8;
			}
		}

		return true;
	}

	bool BitWriter::Write(const uint8_t *data, uint32_t length)
	{
		if (_bit_count % 8 != 0)
		{
			// Writing data is only possible when the bit count is a multiple of 8.
			return false;
		}

		auto current_length = _bit_count / 8;

		if (current_length + length > _data->size())
		{
			// If the data to be written exceeds the capacity, it is not possible to write.
			return false;
		}

		_data->insert(_data->begin() + current_length, data, data + length);

		_bit_count += length * 8;

		return true;
	}
	
}  // namespace ov