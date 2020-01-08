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

bool RtmpStream::ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	return _bsfv.Convert(data, cts);
}


bool RtmpStream::ConvertToAudioData(const std::shared_ptr<ov::Data> &data)
{
	_bsfa.convert_to(data);
	return true;
}
