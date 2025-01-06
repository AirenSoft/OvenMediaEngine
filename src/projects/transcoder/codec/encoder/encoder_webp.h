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

class EncoderWEBP : public TranscodeEncoder
{
public:
	EncoderWEBP(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_WEBP;
	}

	int GetSupportedFormat() const noexcept override
	{
		return AV_PIX_FMT_YUV420P;
	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept override
	{
		return cmn::BitstreamFormat::WEBP;
	}
	
	bool Configure(std::shared_ptr<MediaTrack> context) override;

	bool InitCodec() override;

private:
	bool SetCodecParams() override;	
};
