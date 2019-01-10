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
#include "include/codec_api.h"

#define OPENH264_VER_AT_LEAST(maj, min) \
    ((OPENH264_MAJOR  > (maj)) || \
     (OPENH264_MAJOR == (maj) && OPENH264_MINOR >= (min)))

class OvenCodecImplAvcodecDecAVC : public TranscodeDecoder
{
public:

	~OvenCodecImplAvcodecDecAVC() override;

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;

private:
	ISVCDecoder* _decoder;
	bool _format_changed;
};
