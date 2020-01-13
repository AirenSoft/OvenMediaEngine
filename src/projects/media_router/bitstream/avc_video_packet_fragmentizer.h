#pragma once

#include <base/common_types.h>

#include "base/media_route/media_buffer.h"

#include <cstdint>
#include <memory>
class AvcVideoPacketFragmentizer
{
public:
	bool MakeHeader(std::shared_ptr<MediaPacket> packet);
	
#if 1
	FragmentationHeader* FromAvcVideoPacket2(std::shared_ptr<MediaPacket> packet);
	FragmentationHeader* FromAvcVideoPacket(const uint8_t *packet, size_t length);
#endif

private:
    uint8_t nal_length_size_ = 0;
    size_t continuation_length_ = 0;
};