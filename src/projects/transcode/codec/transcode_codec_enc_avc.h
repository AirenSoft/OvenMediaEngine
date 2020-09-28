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

class OvenCodecImplAvcodecEncAVC : public TranscodeEncoder
{
public:
	~OvenCodecImplAvcodecEncAVC();

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::shared_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

	void ThreadEncode() override;

	void Stop() override;

private:
	// std::shared_ptr<MediaPacket> MakePacket() const;

	// Used to convert output timebase -> codec timebase
	double _scale;
	// Used to convert codec timebase -> output timebase
	double _scale_inv;
};
