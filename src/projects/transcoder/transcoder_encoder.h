//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "codec/codec_base.h"

class TranscodeEncoder : public TranscodeBase<MediaFrame, MediaPacket>
{
public:
	typedef std::function<void(int32_t, std::shared_ptr<MediaPacket>)> _cb_func;

public:
	TranscodeEncoder();
	~TranscodeEncoder() override;

	static std::shared_ptr<TranscodeEncoder> Create(int32_t encoder_id, std::shared_ptr<MediaTrack> output_track, _cb_func on_complete_handler);
	void SetEncoderId(int32_t encdoer_id);

	virtual int GetPixelFormat() const noexcept = 0;

	bool Configure(std::shared_ptr<MediaTrack> output_track) override;

	void SendBuffer(std::shared_ptr<const MediaFrame> frame) override;
	void SendOutputBuffer(std::shared_ptr<MediaPacket> packet);

	std::shared_ptr<MediaTrack> &GetRefTrack();

	virtual void CodecThread() = 0;

	virtual void Stop();

	cmn::Timebase GetTimebase() const;

	_cb_func _on_complete_handler;
	void SetOnCompleteHandler(_cb_func func)
	{
		_on_complete_handler = move(func);
	}

private:
	virtual bool SetCodecParams() = 0;

protected:
	std::shared_ptr<MediaTrack> _track = nullptr;

	int32_t _encoder_id;

	AVCodecContext *_codec_context = nullptr;
	AVCodecParserContext *_parser = nullptr;
	AVCodecParameters *_codec_par = nullptr;

	bool _change_format = false;

	AVPacket *_packet = nullptr;
	AVFrame *_frame = nullptr;

	bool _kill_flag = false;
	std::thread _codec_thread;
};
