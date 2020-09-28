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

class OvenCodecImplAvcodecEncHEVC : public TranscodeEncoder
{
public:
	~OvenCodecImplAvcodecEncHEVC();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H265;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::shared_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

	void ThreadEncode() override;

	void Stop() override;

private:
	// Used to convert output timebase -> codec timebase
	double _scale;
	// Used to convert codec timebase -> output timebase
	double _scale_inv;
};
