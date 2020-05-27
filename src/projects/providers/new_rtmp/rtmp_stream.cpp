//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"
#include "base/info/application.h"

std::shared_ptr<RtmpStream> RtmpStream::Create(const std::shared_ptr<PushProvider> &provider)
{
    auto stream = std::make_shared<RtmpStream>(provider);
	if(stream != nullptr)
	{
		stream->Start();
	}
    return stream;
}

RtmpStream::RtmpStream(const std::shared_ptr<PushProvider> &provider)
	: pvd::PushProviderStream(provider)
{
	
}

RtmpStream::~RtmpStream()
{
}

bool RtmpStream::Start()
{
	_state = Stream::State::PLAYING;
	return true;
}

bool RtmpStream::Stop()
{
	_state = Stream::State::STOPPING;
	return true;
}

bool RtmpStream::ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	return _bsfv.Convert(data, cts);
}


uint32_t RtmpStream::ConvertToAudioData(const std::shared_ptr<ov::Data> &data)
{
	return _bsfa.convert_to(data);
}
