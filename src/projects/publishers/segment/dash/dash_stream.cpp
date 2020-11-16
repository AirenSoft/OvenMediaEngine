//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_stream.h"
#include "dash_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<DashStream> DashStream::Create(int segment_count,
                                              int segment_duration,
                                              const std::shared_ptr<pub::Application> application,
                                              const info::Stream &info,
                                              uint32_t worker_count)
{
	// Check codec compatibility
	bool supported_codec_available = false;
	auto tracks = info.GetTracks();
	for(const auto &track : tracks)
	{
		if(track.second->GetCodecId() == cmn::MediaCodecId::H264 ||
			// If H.265 is supported in the future, this comment should be removed.
			// track.second->GetCodecId() == cmn::MediaCodecId::H265 || 
			track.second->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			supported_codec_available = true;
			break;
		}
	}

	if(supported_codec_available == false)
	{
		logtw("The %s/%s stream has not created because there is no codec that can support it.", info.GetApplicationInfo().GetName().CStr(), info.GetName().CStr());
		return nullptr;
	}

    auto stream = std::make_shared<DashStream>(application, info);

    if (!stream->Start(segment_count, segment_duration))
    {
        return nullptr;
    }
    return stream;
}


//====================================================================================================
// SegmentStream
// - DASH/HLS : H264/AAC only
// TODO : 다중 트랜스코딩/다중 트랙 구분 및 처리 필요
//====================================================================================================
DashStream::DashStream(const std::shared_ptr<pub::Application> application, const info::Stream &info)
                    : SegmentStream(application, info)
{

}

DashStream::~DashStream()
{

}
