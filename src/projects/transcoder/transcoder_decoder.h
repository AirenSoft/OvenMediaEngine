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
	typedef std::function<void(TranscodeResult, int32_t, std::shared_ptr<MediaFrame>)> CompleteHandler;

public:
	TranscodeDecoder(info::Stream stream_info);
	~TranscodeDecoder() override;

	static std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> GetCandidates(bool hwaccels_enable, ov::String hwaccles_moduels, std::shared_ptr<MediaTrack> track);
	static std::shared_ptr<TranscodeDecoder> Create(int32_t decoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> track, std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidates, CompleteHandler complete_handler);

	void SetDecoderId(int32_t decoder_id);
	
	bool Configure(std::shared_ptr<MediaTrack> track) override;

	void SendBuffer(std::shared_ptr<const MediaPacket> packet) override;
	void Complete(TranscodeResult result, std::shared_ptr<MediaFrame> frame);
	
	std::shared_ptr<MediaTrack> &GetRefTrack();

	cmn::Timebase GetTimebase();

	virtual void CodecThread() = 0;

	virtual void Stop();

	void SetCompleteHandler(CompleteHandler complete_handler)
	{
		_complete_handler = move(complete_handler);
	}

protected:
	int32_t _decoder_id;
	int32_t _gpu_id = -1;

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

	CompleteHandler _complete_handler;
};
