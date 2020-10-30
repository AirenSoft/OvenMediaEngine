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

class OvenCodecImplAvcodecEncJpeg : public TranscodeEncoder
{
public:
	~OvenCodecImplAvcodecEncJpeg();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_MJPEG;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::shared_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

	void ThreadEncode() override;

	void Stop() override;
};
