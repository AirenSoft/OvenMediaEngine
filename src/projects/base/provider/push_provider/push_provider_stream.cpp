//==============================================================================
//
//  PushProviderStream
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "push_provider_application.h"
#include "push_provider_private.h"

namespace pvd
{
	PushProviderStream::PushProviderStream(StreamSourceType source_type, const std::shared_ptr<ov::Socket> &remote)
		:Stream(source_type)
	{
	}

	
}