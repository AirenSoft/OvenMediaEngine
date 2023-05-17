//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <math.h>
#include <base/ovlibrary/ovlibrary.h>

namespace ov
{
	class BitWriter
	{
	public :
		explicit BitWriter(uint32_t data_size);
		~BitWriter() = default;

	public :
		bool 			Write(uint32_t bit_count, uint64_t value);
		// It works only _bit_count is multiple of 8.
		bool 			Write(const uint8_t *data, uint32_t length);

		uint32_t 		GetBitCount(){ return _bit_count; }
		const uint8_t*	GetData() { return _data->data(); }
		size_t			GetDataSize(){ return (size_t)ceil((double)_bit_count / 8); }
		size_t 			GetCapacity() { return _data->size(); }

		std::shared_ptr<ov::Data> GetDataObject()
		{
			return std::make_shared<ov::Data>(_data->data(), GetDataSize());
		}

	private :
		std::shared_ptr<std::vector<uint8_t>> _data;
		uint32_t  _bit_count;
	};
} // namespace ov
