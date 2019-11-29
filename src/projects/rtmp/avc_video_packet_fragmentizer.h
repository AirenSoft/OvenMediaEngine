#pragma once

#include <base/common_types.h>

#include <cstdint>
#include <memory>
class AvcVideoPacketFragmentizer
{
public:
	std::unique_ptr<FragmentationHeader> FromAvcVideoPacket(const uint8_t *packet, size_t length);

private:
    uint8_t nal_length_size_ = 0;
    size_t continuation_length_ = 0;
};