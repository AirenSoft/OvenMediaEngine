//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../transcoder_encoder.h"

class EncoderVP8 : public TranscodeEncoder
{
public:
	~EncoderVP8();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_VP8;
	}

	int GetPixelFormat() const noexcept override
	{
		return AV_PIX_FMT_YUV420P;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;	
};
