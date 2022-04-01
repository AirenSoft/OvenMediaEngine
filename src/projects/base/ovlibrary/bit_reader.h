#pragma once

#include <utility>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <base/ovlibrary/ovlibrary.h>

#include "byte_io.h"

class BitReader
{

public:
    BitReader(const uint8_t *buffer, size_t capacity) : 
		_buffer(buffer),
        _position(buffer),
        _capacity(capacity)
    {
    }

	template<typename T>
	T ReadBytes(bool big_endian = true)
	{
		T value;
		bool result = ReadBytes(value, big_endian);
		if(result == false)
		{
			return 0;
		}

		return value;
	}

	bool SkipAll()
	{
		return SkipBytes(BytesReamined());
	}

	bool SkipBytes(size_t length)
	{
		if (length > static_cast<size_t>(_capacity - (_position - _buffer))) 
		{
			return false;
		}

		_position += length;

		return true;
	}

	ov::String ReadString(size_t length)
	{
		length = std::min(length, BytesReamined());

		ov::String str(reinterpret_cast<const char *>(_position), length);

		_position += length;

		return str;
	}

	// Note: ReadBytes() API obtains the bits without considering _bit_offset
	template<typename T>
	bool ReadBytes(T& value, bool big_endian = true)
	{
		if (sizeof(value) > static_cast<size_t>(_capacity - (_position - _buffer))) 
		{
			return false;
		}

		if(big_endian == true)
		{
			value = ByteReader<T>::ReadBigEndian(_position);
		}
		else
		{
			value = ByteReader<T>::ReadLittleEndian(_position);
		}

		_position += sizeof(value);

		return true;
	}

	template<typename T>
    T ReadBits(uint8_t bits)
	{
		T value;
		bool result = ReadBits(bits, value);
		if(result == false)
		{
			return 0;
		}

		return value;
	}

    template<typename T>
    bool ReadBits(uint8_t bits, T& value)
    {
        if (bits > sizeof(value) * 8)
        {
            OV_ASSERT2(false);
            return false;
        }
        value = 0;
        if (bits == 0) 
		{
			return 0;
		}

        if (static_cast<size_t>((bits + 7) / 8) > static_cast<size_t>(_capacity - (_position - _buffer))) 
		{
			return false;
		}

        while (bits)
        {
            const uint8_t bits_from_this_byte = std::min(bits >= 8 ? 8 : bits % 8, 8 - _bit_offset);
            const uint8_t mask_offset = 8 - bits_from_this_byte - _bit_offset;
            const uint8_t mask = ((1 << bits_from_this_byte ) - 1) << mask_offset;
            value <<= bits_from_this_byte;
            value |=  (*_position & mask) >> mask_offset;
            bits -= bits_from_this_byte;
            _bit_offset += bits_from_this_byte;
            if (_bit_offset == 8)
            {
                ++_position;
                _bit_offset = 0;
            }
        }
        return true;
    }

    bool ReadBoolBit()
	{
		bool value;
		bool result = ReadBit(value);
		if(result == false)
		{
			return false;
		}

		return value;
	}

    bool ReadBit(bool &value)
    {
        uint8_t bit;
        if (ReadBit(bit))
        {
            value = bit == 1 ? true : false;
            return true;
        }
        return false;
    }

	uint8_t ReadBit()
	{
		uint8_t value;
		bool result = ReadBit(value);
		if(result == false)
		{
			return 0;
		}

		return value;
	}

    bool ReadBit(uint8_t &value)
    {
        if (static_cast<size_t>(_position - _buffer) == _capacity) return false;
        value = *_position & (1  << (7 - _bit_offset)) ? 1 : 0;
        if (_bit_offset == 7)
        {
            ++_position;
            _bit_offset = 0;
        }
        else
        {
            ++_bit_offset;
        }
        return true;
    }

	void StartSection()
	{
		_lap_position = _position;
	}

	size_t BytesSetionConsumed()
	{
		if(_lap_position == nullptr)
		{
			return 0;
		}

		auto bytes = _position - _lap_position;
		return bytes;
	}

	const uint8_t* CurrentPosition()
	{
		return _position;
	}

	size_t BytesReamined() const
	{
		return _capacity - BytesConsumed();
	}

	size_t BitsRemained() const
	{
		return (_capacity * 8) - BitsConsumed();
	}

    size_t BytesConsumed() const
    {
        return _position - _buffer;
    }

    size_t BitsConsumed() const
    {
        return (BytesConsumed() * 8) + _bit_offset;
    }

protected:
    const uint8_t* _buffer;
    const uint8_t* _position;
	const uint8_t* _lap_position;
    size_t _capacity;
    int _bit_offset = 0;
    uint8_t _mask = 0x80;
};
