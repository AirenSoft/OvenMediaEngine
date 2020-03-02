#pragma once

#include <utility>
#include <cstdint>
#include <type_traits>
#include <algorithm>

class BitReader
{

public:
    BitReader(const uint8_t *buffer, size_t capacity) : buffer_(buffer),
        position_(buffer),
        capacity_(capacity),
        bit_offset_(0),
        mask_(0x80)
    {
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
        if (bits == 0) return 0;
        if (static_cast<size_t>((bits + 7) / 8) > static_cast<size_t>(capacity_ - (position_ - buffer_))) return false;
        while (bits)
        {
            const uint8_t bits_from_this_byte = std::min(bits > 8 ? 8 : bits % 8, 8 - bit_offset_);
            const uint8_t mask_offset = 8 - bits_from_this_byte - bit_offset_;
            const uint8_t mask = ((1 << bits_from_this_byte ) - 1) << mask_offset;
            value <<= bits_from_this_byte;
            value |=  (*position_ & mask) >> mask_offset;
            bits -= bits_from_this_byte;
            bit_offset_ += bits_from_this_byte;
            if (bit_offset_ == 8)
            {
                ++position_;
                bit_offset_ = 0;
            }
        }
        return true;
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

    bool ReadBit(uint8_t &value)
    {
        if (static_cast<size_t>(position_ - buffer_) == capacity_) return false;
        value = *position_ & (1  << (7 - bit_offset_)) ? 1 : 0;
        if (bit_offset_ == 7)
        {
            ++position_;
            bit_offset_ = 0;
        }
        else
        {
            ++bit_offset_;
        }
        return true;
    }

    size_t BytesConsumed() const
    {
        return position_ - buffer_;
    }

    size_t BitsConsumed() const
    {
        return (position_ - buffer_) * 8 + bit_offset_;
    }

private:
    const uint8_t *const buffer_;
    const uint8_t* position_;
    const size_t capacity_;
    int bit_offset_;
    uint8_t mask_;
};
