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

class EncoderFFOPUS : public TranscodeEncoder
{
public:
	EncoderFFOPUS(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_OPUS;
	}

	int GetSupportedFormat() const noexcept override 
	{
		return AV_SAMPLE_FMT_S16;
	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept override
	{
		return cmn::BitstreamFormat::OPUS;
	}
	
	bool Configure(std::shared_ptr<MediaTrack> output_context) override;

	bool InitCodec() override;

private:
	bool SetCodecParams() override;	
};
