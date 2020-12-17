//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_stream.h"

#include "hls_private.h"

std::shared_ptr<HlsStream> HlsStream::Create(int segment_count, int segment_duration,
											 const std::shared_ptr<pub::Application> &application,
											 const info::Stream &info,
											 uint32_t worker_count)
{
	// Check codec compatibility
	bool supported_codec_found = false;

	for (const auto &track : info.GetTracks())
	{
		auto codec_id = track.second->GetCodecId();

		switch (codec_id)
		{
			case cmn::MediaCodecId::H264:
			case cmn::MediaCodecId::H265:
			case cmn::MediaCodecId::Aac:
				supported_codec_found = true;
				break;

			default:
				break;
		}
	}

	if (supported_codec_found == false)
	{
		logtw("The %s/%s stream has not created because there is no codec that can support it.",
			  info.GetApplicationInfo().GetName().CStr(), info.GetName().CStr());

		return nullptr;
	}

	auto stream = std::make_shared<HlsStream>(application, info);

	if (stream->Start(segment_count, segment_duration) == false)
	{
		return nullptr;
	}

	return stream;
}

HlsStream::HlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info)
	: SegmentStream(application, info)
{
}

HlsStream::~HlsStream()
{
}