//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "stream.h"
#include "application.h"
#include "base/info/application.h"

namespace pvd
{
	Stream::Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type)
		:info::StreamInfo(*(std::static_pointer_cast<info::Application>(application)), source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type)
		:info::StreamInfo((*std::static_pointer_cast<info::Application>(application)), stream_id, source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, const StreamInfo &stream_info)
		:info::StreamInfo(stream_info),
		_application(application)
	{
	}

	Stream::~Stream()
	{

	}
}