//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value_container.h"

namespace cfg
{
	// For specialized classes
	template<typename Ttype, typename Tenabler = void>
	class Value : public ValueContainer<Ttype>
	{
	};
}