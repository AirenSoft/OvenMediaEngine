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
#include "codec/codec_base.h"

class TranscodeDecoder : public TranscodeBase<MediaPacket, MediaFrame>
{
public:
	TranscodeDecoder(info::Stream stream_info);
	~TranscodeDecoder() override;

	static std::shared_ptr<TranscodeDecoder> CreateDecoder(const info::Stream &info, std::shared_ptr<TranscodeContext> context);

	void SetTrackId(int32_t track_id);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;
	void SendOutputBuffer(bool change_format, int32_t track_id, std::shared_ptr<MediaFrame> frame);

	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;

	std::shared_ptr<TranscodeContext> &GetContext();

	cmn::Timebase GetTimebase() const;

	virtual void CodecThread() = 0;

	virtual void Stop();

	typedef std::function<void(TranscodeResult, int32_t)> _cb_func;
	_cb_func OnCompleteHandler;
	void SetOnCompleteHandler(_cb_func func)
	{
		OnCompleteHandler = move(func);
	}

protected:
	static const ov::String ShowCodecParameters(const AVCodecContext *context, const AVCodecParameters *parameters);

	std::shared_ptr<TranscodeContext> _input_context;
	int32_t _track_id;

	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;

	info::Stream _stream_info;

	bool _kill_flag = false;
	std::thread _codec_thread;
};
