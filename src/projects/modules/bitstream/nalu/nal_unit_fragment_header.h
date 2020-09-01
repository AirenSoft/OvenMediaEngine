#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"

class NalUnitFragmentHeader
{
public:
	NalUnitFragmentHeader();
	~NalUnitFragmentHeader();

	static bool Parse(const std::shared_ptr<MediaPacket> &packet);
};
