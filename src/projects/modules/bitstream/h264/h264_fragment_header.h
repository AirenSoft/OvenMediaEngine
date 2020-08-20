#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"

class H264FragmentHeader
{
public:
	H264FragmentHeader();
	~H264FragmentHeader();

	static bool Parse(const std::shared_ptr<MediaPacket> &packet);
};
