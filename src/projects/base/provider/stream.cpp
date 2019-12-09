//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "stream.h"

namespace pvd
{
	Stream::Stream()
	{
	}

	Stream::Stream(uint32_t stream_id)
		:StreamInfo(stream_id)
	{
	}

	Stream::Stream(const StreamInfo &stream_info)
		:StreamInfo(stream_info)
	{
	}

	Stream::~Stream()
	{

	}
}