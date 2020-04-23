//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"
#include "base/info/application.h"

std::shared_ptr<RtmpStream> RtmpStream::Create(const std::shared_ptr<pvd::Application> &application, const uint32_t stream_id, const ov::String &stream_name)
{
	info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::Rtmp);
	stream_info.SetId(stream_id);
	stream_info.SetName(stream_name);

    auto stream = std::make_shared<RtmpStream>(application, stream_info);
	if(stream != nullptr)
	{
		stream->Start();
	}
    return stream;
}

RtmpStream::RtmpStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
	: pvd::Stream(application, stream_info)
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
