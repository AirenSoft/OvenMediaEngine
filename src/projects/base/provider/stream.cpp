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
	Stream::Stream(StreamSourceType source_type)
		:StreamInfo(source_type)
	{
	}

	Stream::Stream(uint32_t stream_id, StreamSourceType source_type)
		:StreamInfo(stream_id, source_type)
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