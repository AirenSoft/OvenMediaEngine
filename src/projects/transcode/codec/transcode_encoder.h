//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_base.h"

class TranscodeEncoder : public TranscodeBase<MediaFrame, MediaPacket>
{
public:
	TranscodeEncoder();
	~TranscodeEncoder() override;

	static std::shared_ptr<TranscodeEncoder> CreateEncoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> output_context);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaFrame> frame) override;

protected:
	std::shared_ptr<TranscodeContext> _output_context = nullptr;

	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_packet = nullptr;
	AVFrame *_frame = nullptr;

	int _decoded_frame_num = 0;
};
