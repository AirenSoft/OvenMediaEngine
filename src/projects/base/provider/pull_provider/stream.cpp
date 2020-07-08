//==============================================================================
//
//  PullProvider Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "stream.h"
#include "application.h"
#include "base/info/application.h"
#include "provider_private.h"

namespace pvd
{
	PullStream::PullStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		: Stream(application, stream_info)
	{

	}

	bool PullStream::Start()
	{
		return Stream::Start();
	}

	bool PullStream::Stop()
	{
		return Stream::Stop();
	}

	bool PullStream::Play()
	{
		logti("%s has started to play [%s(%u)] stream", GetApplicationTypeName(), GetName().CStr(), GetId());
		return true;
	}
}