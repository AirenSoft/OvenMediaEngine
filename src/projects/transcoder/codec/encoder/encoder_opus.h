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

struct OpusEncoder;

class EncoderOPUS : public TranscodeEncoder
{
public:
	EncoderOPUS(const info::Stream &stream_info)
		: TranscodeEncoder(stream_info)
	{
	}

	~EncoderOPUS() override;

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
	
	bool Configure(std::shared_ptr<MediaTrack> context) override;

	bool InitCodec() override;

	// void SendBuffer(std::shared_ptr<const MediaFrame> frame) override;

	void CodecThread() override;

private:
	bool SetCodecParams() override;
	
protected:
	std::shared_ptr<ov::Data> _buffer;
	int _expert_frame_duration;
	cmn::AudioSample::Format _format;
	int64_t _current_pts;
	uint32_t _frame_size = 0;

	OpusEncoder *_encoder;
};
