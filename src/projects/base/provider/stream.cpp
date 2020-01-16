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
	Stream::Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type)
		:StreamInfo(source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type)
		:StreamInfo(stream_id, source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, const StreamInfo &stream_info)
		:StreamInfo(stream_info),
		_application(application)
	{
	}

	Stream::~Stream()
	{

	}
}