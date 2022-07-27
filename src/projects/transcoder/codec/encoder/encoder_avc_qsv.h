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

class EncoderAVCxQSV : public TranscodeEncoder
{
public:
	~EncoderAVCxQSV();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	int GetSupportedFormat() const noexcept override 
	{
		return AV_PIX_FMT_NV12;
	}

	bool Configure(std::shared_ptr<MediaTrack> context) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;	
};
