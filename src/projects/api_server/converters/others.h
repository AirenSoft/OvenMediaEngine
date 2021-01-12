//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace api
{
	namespace conv
	{
		Json::Value JsonFromError(const std::shared_ptr<ov::Error> &error);
	}
}