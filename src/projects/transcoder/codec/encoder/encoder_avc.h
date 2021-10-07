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

class EncoderAVC : public TranscodeEncoder
{
public:
	~EncoderAVC();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	int GetPixelFormat() const noexcept override
	{
		return AV_PIX_FMT_YUV420P;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::shared_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

	void ThreadEncode() override;

	void Stop() override;

private:
	bool SetCodecParams() override;
};
