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

	static std::shared_ptr<TranscodeEncoder> CreateEncoder(cmn::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> output_context);
	void SetTrackId(int32_t track_id);

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	void SendBuffer(std::shared_ptr<const MediaFrame> frame) override;
	void SendOutputBuffer(std::shared_ptr<MediaPacket> packet);

	std::shared_ptr<TranscodeContext>& GetContext();

	virtual void ThreadEncode();

	virtual void Stop();

	cmn::Timebase GetTimebase() const;

	// TODO(soulk): The encoder and decoder are also changed to the way callback is called 
	// when the encoder and decoder are completed.
	typedef std::function<TranscodeResult(int32_t)> _cb_func;
	_cb_func OnCompleteHandler;
	void SetOnCompleteHandler(_cb_func func) {
		OnCompleteHandler = move(func);
	}

protected:
	std::shared_ptr<TranscodeContext> _output_context = nullptr;

	int32_t _track_id;

	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_packet = nullptr;
	AVFrame *_frame = nullptr;

	int _decoded_frame_num = 0;

	bool _kill_flag = false;
	std::thread _thread_work;

};
