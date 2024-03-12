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

class EncoderHEVCxXMA : public TranscodeEncoder
{
public:
	EncoderHEVCxXMA(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H265;
	}

	int GetSupportedFormat() const noexcept override
	{
#ifdef HWACCELS_XMA_ENABLED
		return AV_PIX_FMT_XVBM_8;
#else	
		// Unknown colorspace	
		return 0;
#endif		

	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept override
	{
		return cmn::BitstreamFormat::H265_ANNEXB;
	}

	bool Configure(std::shared_ptr<MediaTrack> context) override;

private:
	bool SetCodecParams() override;
};
