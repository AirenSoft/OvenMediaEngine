//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "cmaf_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<CmafStream> CmafStream::Create(int segment_count,
                                              int segment_duration,
                                              const std::shared_ptr<pub::Application> application,
                                              const info::Stream &info,
                                              uint32_t worker_count,
                                              const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
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

    auto stream = std::make_shared<CmafStream>(application, info, chunked_transfer);

    if (!stream->Start(segment_count, segment_duration))
    {
        return nullptr;
    }
    return stream;
}


//====================================================================================================
// SegmentStream
// -  H264/AAC only
// TODO : 다중 트랜스코딩/다중 트랙 구분 및 처리 필요
//====================================================================================================
CmafStream::CmafStream(const std::shared_ptr<pub::Application> application,
					   const info::Stream &info,
					   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
					   	: SegmentStream(application, info)
{
	_chunked_transfer = chunked_transfer;
}

CmafStream::~CmafStream()
{

}
