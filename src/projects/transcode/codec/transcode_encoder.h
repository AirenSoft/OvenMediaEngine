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

	static std::unique_ptr<TranscodeEncoder> CreateEncoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> transcode_context = nullptr);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::unique_ptr<const MediaFrame> frame) override;

protected:
	std::shared_ptr<TranscodeContext> _transcode_context = nullptr;

	AVCodecContext *_context;
	AVCodecParserContext *_parser;
	AVCodecParameters *_codec_par;

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;

	int _decoded_frame_num = 0;

	// 디버깅
	uint64_t _coded_frame_count = 0;
	uint64_t _coded_data_size = 0;
};
