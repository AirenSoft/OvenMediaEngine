//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/common_types.h"

namespace mon
{
	enum PublisherType
	{
		OVT = 0,
		WEBRTC,
		HLS,
		LLHLS,
		DASH,
		LLDASH,
		NUMBER_OF_PUBLISHERS
	};
}