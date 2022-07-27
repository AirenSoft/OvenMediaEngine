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

class EncoderAAC : public TranscodeEncoder
{
public:
	~EncoderAAC();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_AAC;
	}

	int GetSupportedFormat() const noexcept override
	{
		return AV_SAMPLE_FMT_S16;
	}

	bool Configure(std::shared_ptr<MediaTrack> output_context) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;
};
