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

class DecoderAVCxNIQUADRA : public TranscodeDecoder
{
public:
	DecoderAVCxNIQUADRA(const info::Stream &stream_info)
		: TranscodeDecoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	bool Configure(std::shared_ptr<MediaTrack> context) override;
	
	bool InitCodec();
	//void UninitCodec();
	//bool ReinitCodecIfNeed();

	void CodecThread() override;
};
