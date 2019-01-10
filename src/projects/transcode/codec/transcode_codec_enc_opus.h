//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_encoder.h"

struct OpusEncoder;

class OvenCodecImplAvcodecEncOpus : public TranscodeEncoder
{
public:

	~OvenCodecImplAvcodecEncOpus() override;

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_OPUS;
	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::unique_ptr<const MediaFrame> frame) override;

	std::unique_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

protected:
	std::shared_ptr<ov::Data> _buffer;

	common::AudioSample::Format _format;
	int64_t _current_pts;

	OpusEncoder *_encoder;
};
