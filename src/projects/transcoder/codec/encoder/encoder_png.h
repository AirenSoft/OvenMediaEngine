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

class EncoderPNG : public TranscodeEncoder
{
public:
	~EncoderPNG();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_PNG;
	}

	int GetSupportedFormat() const noexcept override
	{
		return AV_PIX_FMT_RGBA;
	}

	bool Configure(std::shared_ptr<MediaTrack> context) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;	
};
