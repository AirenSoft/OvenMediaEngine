#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "base/mediarouter/media_buffer.h"
#include <cstdint>

class H264AvccToAnnexB
{
public:
	static bool GetExtradata(const common::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata);
	static bool Convert(common::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata);
};
