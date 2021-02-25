//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_stream.h"

namespace pvd
{
	std::shared_ptr<WebRTCStream> WebRTCStream::Create(StreamSourceType source_type)
	{
		return nullptr;
	}
	
	WebRTCStream::WebRTCStream(StreamSourceType source_type)
		: pvd::Stream(source_type)
	{

	}

	WebRTCStream::~WebRTCStream()
	{

	}

	bool WebRTCStream::Start()
	{
		return pvd::Stream::Start();
	}

	bool WebRTCStream::Stop()
	{
		return pvd::Stream::Stop();
	}
}