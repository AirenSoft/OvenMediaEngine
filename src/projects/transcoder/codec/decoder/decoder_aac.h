//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../transcoder_decoder.h"

class DecoderAAC : public TranscodeDecoder
{
public:
	DecoderAAC(const info::Stream &stream_info)
		: TranscodeDecoder(stream_info)
	{
	}

	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_AAC;
	}

	std::shared_ptr<const MediaPacket> _cur_pkt = nullptr;
	size_t _pkt_offset = 0;
	std::shared_ptr<const ov::Data> _cur_data = nullptr;

	int64_t _last_pkt_pts = 0;

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void CodecThread() override;

protected:
};
