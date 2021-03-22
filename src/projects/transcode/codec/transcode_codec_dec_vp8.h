//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_decoder.h"

class OvenCodecImplAvcodecDecVP8 : public TranscodeDecoder
{
public:
	OvenCodecImplAvcodecDecVP8(const info::Stream &stream_info)
		: TranscodeDecoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_VP8;
	}
	void ThreadDecode() override;

	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;
};
