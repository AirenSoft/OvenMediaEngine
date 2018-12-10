//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "bit_writer.h"

//====================================================================================================
// Constructor
//====================================================================================================
BitWriter::BitWriter(uint32_t data_size)
{
	_data = std::make_unique<std::vector<uint8_t>>(data_size, 0);
	_bit_count = 0; 
}

//====================================================================================================
// Destructor
//====================================================================================================
void BitWriter::Write(uint32_t bit_count, uint32_t value)
{
	uint8_t     *data 	= nullptr;
	uint32_t    space 	= 0;
	
	data = _data->data();
	
	if (_bit_count + bit_count > _data->size()*8)
	{
		return;
	}

	data += _bit_count/8;

	space = 8-(_bit_count%8);

	while (bit_count) 
	{
		uint32_t mask = bit_count==32 ? 0xFFFFFFFF : ((1<<bit_count)-1);
		
		if (bit_count <= space)
		{
			*data |= ((value&mask) << (space -bit_count));
			_bit_count += bit_count;
			return;
		}
		else
		{
			*data |= ((value&mask) >> (bit_count- space));
			++data;
			_bit_count	+= space;
			bit_count	-= space;
			space		= 8;
		}
	}
}