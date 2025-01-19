//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../transcoder_decoder.h"
#include <list>

class DecoderHEVCxXMA : public TranscodeDecoder
{
public:
	DecoderHEVCxXMA(const info::Stream &stream_info)
		: TranscodeDecoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H265;
	}

	bool InitCodec();
	void UninitCodec();
	bool ReinitCodecIfNeed();

	void CodecThread() override;

private:
	[[maybe_unused]]
	std::list<int64_t> _pts_reorder_list;	
};
