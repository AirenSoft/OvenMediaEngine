//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/info/stream_info.h"

namespace pvd
{
	class Stream : public StreamInfo
	{
	protected:
		Stream();
		Stream(uint32_t stream_id);
		Stream(const StreamInfo &stream_info);

		virtual ~Stream();
	};
}