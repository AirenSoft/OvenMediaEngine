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

class EncoderAVCxOpenH264 : public TranscodeEncoder
{
public:
	~EncoderAVCxOpenH264();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
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
