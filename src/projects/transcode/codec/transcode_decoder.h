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

	void SetTrackId(int32_t track_id);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;

	std::shared_ptr<TranscodeContext> &GetContext();

	cmn::Timebase GetTimebase() const;

	virtual void ThreadDecode() = 0;

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

	int _decoded_frame_num = 0;

	info::Stream _stream_info;

	bool _kill_flag = false;
	std::thread _thread_work;
};
