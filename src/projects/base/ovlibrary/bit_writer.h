//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <stdint.h>
#include <vector>
#include <memory>
#include <math.h>
#pragma once

namespace ov
{
	class BitWriter
	{
	public :
		explicit BitWriter(uint32_t data_size);
		~BitWriter() = default;

	public :
		void 			Write(uint32_t bit_count, uint32_t value);
		uint32_t 		GetBitCount(){ return _bit_count; }
		const uint8_t*	GetData() { return _data->data(); }
		size_t			GetDataSize(){ return (size_t)ceil((double)_bit_count / 8); }
		size_t 			GetCapacity() { return _data->size(); }
	private :
		std::shared_ptr<std::vector<uint8_t>> _data;
		uint32_t  _bit_count;
	};
} // namespace ov
