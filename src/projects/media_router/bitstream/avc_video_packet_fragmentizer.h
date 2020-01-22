#pragma once

#include <base/common_types.h>
#include "base/media_route/media_buffer.h"

#include <cstdint>
#include <memory>

class AvcVideoPacketFragmentizer
{
public:
	bool MakeHeader(const std::shared_ptr<MediaPacket> &packet);

private:
    uint8_t nal_length_size_ = 0;
    size_t continuation_length_ = 0;
};