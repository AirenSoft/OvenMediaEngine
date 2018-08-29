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

class OvenCodecImplAvcodecEncAAC : public TranscodeEncoder
{
public:
	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_AAC;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::unique_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;
};
