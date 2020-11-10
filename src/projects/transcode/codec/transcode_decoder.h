//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/info/stream.h"
#include "transcode_base.h"

class TranscodeDecoder : public TranscodeBase<MediaPacket, MediaFrame>
{
public:
	TranscodeDecoder(info::Stream stream_info);
	~TranscodeDecoder() override;

	static std::shared_ptr<TranscodeDecoder> CreateDecoder(const info::Stream &info, cmn::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> input_context);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;

	std::shared_ptr<TranscodeContext>& GetContext();

	cmn::Timebase GetTimebase() const;

protected:
	static const ov::String ShowCodecParameters(const AVCodecContext *context, const AVCodecParameters *parameters);

	std::shared_ptr<TranscodeContext> _input_context;

	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;
	int _decoded_frame_num = 0;

	info::Stream		_stream_info;
};
