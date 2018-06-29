//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <cstdint>
#include "string.h"

namespace ov
{
	class Random
	{
	public:
		Random() = default;
		virtual ~Random() = default;

		static uint32_t 	GenerateInteger();
		static ov::String	GenerateString(uint32_t length);
	};
}