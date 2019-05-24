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

class TranscodeDecoder : public TranscodeBase<MediaPacket, MediaFrame>
{
public:
	TranscodeDecoder();
	~TranscodeDecoder() override;

	static std::unique_ptr<TranscodeDecoder> CreateDecoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> transcode_context = nullptr);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::unique_ptr<const MediaPacket> packet) override;

protected:
	void ShowCodecParameters(const AVCodecParameters *parameters);

	AVCodec *_codec = nullptr;
	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = avcodec_parameters_alloc();

	std::shared_ptr<TranscodeContext> _transcode_context;

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;
	int _decoded_frame_num = 0;
};
