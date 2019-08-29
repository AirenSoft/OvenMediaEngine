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

	static std::unique_ptr<TranscodeEncoder> CreateEncoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> output_context);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::unique_ptr<const MediaFrame> frame) override;

protected:
	std::shared_ptr<TranscodeContext> _output_context = nullptr;

	AVCodecContext *_context;
	AVCodecParserContext *_parser;
	AVCodecParameters *_codec_par;

	bool _change_format = false;

	AVPacket *_packet;
	AVFrame *_frame;

	int _decoded_frame_num = 0;
};
