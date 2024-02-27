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

class EncoderAVCxNIQUADRA : public TranscodeEncoder
{
public:
	EncoderAVCxNIQUADRA(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	int GetSupportedFormat() const noexcept override
	{
        /*
         * Options:
         * AV_PIX_FMT_NI_QUAD - Hardware frame format
         * AV_PIX_FMT_YUV420P - 8 Bit Software
         * AV_PIX_FMT_YUV420P10LE - 10 Bit Software
         * AV_PIX_FMT_NV12 - CUDA Frame
         * AV_PIX_FMT_P010LE - ?
         */
		return AV_PIX_FMT_NI_QUAD;
	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept override
	{
		return cmn::BitstreamFormat::H264_ANNEXB;
	}
	
	bool Configure(std::shared_ptr<MediaTrack> context) override;

private:
	bool SetCodecParams() override;	
};
