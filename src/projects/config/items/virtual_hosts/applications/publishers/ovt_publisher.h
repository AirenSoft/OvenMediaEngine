//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"

namespace cfg
{
	struct OvtPublisher : public Publisher
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::Ovt)
	};
}  // namespace cfg