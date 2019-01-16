//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_encoder.h"
#include "include/codec_api.h"

class OvenCodecImplAvcodecEncAVC : public TranscodeEncoder
{
public:

	~OvenCodecImplAvcodecEncAVC() override;

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::unique_ptr<const MediaFrame> frame) override;

	std::unique_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

private:
	ISVCEncoder* _encoder;
};
