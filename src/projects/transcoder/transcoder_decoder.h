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

	typedef std::function<void(TranscodeResult, int32_t, std::shared_ptr<MediaFrame>)> _cb_func;

	static std::shared_ptr<TranscodeDecoder> Create(int32_t decoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> track,  _cb_func func);

	void SetDecoderId(int32_t decoder_id);

	bool Configure(std::shared_ptr<MediaTrack> track) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;
	void SendOutputBuffer(TranscodeResult result, std::shared_ptr<MediaFrame> frame);
	
	std::shared_ptr<MediaTrack> &GetRefTrack();

	cmn::Timebase GetTimebase();

	virtual void CodecThread() = 0;

	virtual void Stop();

	void SetOnCompleteHandler(_cb_func func)
	{
		_on_complete_hander = move(func);
	}


protected:
	static const ov::String ShowCodecParameters(const AVCodecContext *context, const AVCodecParameters *parameters);

	int32_t _decoder_id;

	std::shared_ptr<MediaTrack> _track;

	AVCodecContext *_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_pkt;
	AVFrame *_frame;

	info::Stream _stream_info;

	bool _kill_flag = false;
	std::thread _codec_thread;

	_cb_func _on_complete_hander;
};
