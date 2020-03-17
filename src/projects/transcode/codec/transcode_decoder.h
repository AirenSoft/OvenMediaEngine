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

	static std::shared_ptr<TranscodeDecoder> CreateDecoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> input_context);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;

	std::shared_ptr<TranscodeContext>& GetContext();

protected:
	static void ShowCodecParameters(const AVCodecContext *context, const AVCodecParameters *parameters);

	std::shared_ptr<TranscodeContext> _input_context;

	AVCodec *_codec = nullptr;
	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = avcodec_parameters_alloc();

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;
	int _decoded_frame_num = 0;
};
