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
	EncoderPNG(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_PNG;
	}

	int GetSupportedFormat() const noexcept override
	{
		return AV_PIX_FMT_RGBA;
	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept override
	{
		return cmn::BitstreamFormat::PNG;
	}
	
	bool Configure(std::shared_ptr<MediaTrack> context) override;

private:
	bool SetCodecParams() override;	
};
