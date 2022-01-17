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

class EncoderHEVCxQSV : public TranscodeEncoder
{
public:
	~EncoderHEVCxQSV();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H265;
	}

	int GetPixelFormat() const noexcept override 
	{
		return AV_PIX_FMT_NV12;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;	
};
