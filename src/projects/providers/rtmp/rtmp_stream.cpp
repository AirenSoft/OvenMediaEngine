//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_stream.h"

std::shared_ptr<RtmpStream> RtmpStream::Create(const std::shared_ptr<pvd::Application> &application)
{
    auto stream = std::make_shared<RtmpStream>(application);
    return stream;
}

RtmpStream::RtmpStream(const std::shared_ptr<pvd::Application> &application)
	: pvd::Stream(application, StreamSourceType::Rtmp)
{
	
}


RtmpStream::~RtmpStream()
{
}

bool RtmpStream::ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	return _bsfv.Convert(data, cts);
}


uint32_t RtmpStream::ConvertToAudioData(const std::shared_ptr<ov::Data> &data)
{
	return _bsfa.convert_to(data);
}


void RtmpStream::SetAudioTimestampScale(double scale){
	_audio_timestamp_scale = scale;
}

double RtmpStream::GetAudioTimestampScale()
{
	return _audio_timestamp_scale;
}


void RtmpStream::SetVideoTimestampScale(double scale){
	_video_timestamp_scale = scale;
}

double RtmpStream::GetVideoTimestampScale()
{
	return _video_timestamp_scale;
}