//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"

namespace cfg
{
	struct OvtProvider : public Provider
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(ProviderType, GetType, ProviderType::Ovt)
	};
}