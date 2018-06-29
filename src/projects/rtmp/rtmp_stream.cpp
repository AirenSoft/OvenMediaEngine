//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"

std::shared_ptr<RtmpStream> RtmpStream::Create()
{
    auto stream = std::make_shared<RtmpStream>();
    return stream;
}

RtmpStream::RtmpStream()
{
	
}


RtmpStream::~RtmpStream()
{
}
