//==============================================================================
//
//  PushStream
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "application.h"
#include "stream.h"
#include "provider_private.h"

namespace pvd
{
	PushStream::PushStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		: Stream(application, stream_info)
	{	
	}
	PushStream::PushStream(StreamSourceType source_type, const std::shared_ptr<PushProvider> &provider)
		: Stream(source_type)
	{
	}
}