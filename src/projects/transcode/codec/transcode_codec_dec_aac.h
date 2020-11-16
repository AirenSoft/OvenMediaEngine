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

class OvenCodecImplAvcodecDecAAC : public TranscodeDecoder
{
public:
	OvenCodecImplAvcodecDecAAC(const info::Stream &stream_info)
        : TranscodeDecoder(stream_info)
    {

    }


	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_AAC;
	}

	std::shared_ptr<MediaFrame> Dequeue(TranscodeResult *result);
	void Enqueue(TranscodeResult *result);


	std::shared_ptr<const MediaPacket> _cur_pkt = nullptr;
	size_t _pkt_offset = 0;
	std::shared_ptr<const ov::Data> _cur_data = nullptr;

	int64_t _last_pkt_pts = 0;
	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;

protected:
};
