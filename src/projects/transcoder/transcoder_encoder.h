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

class TranscodeEncoder : public TranscodeBase<MediaFrame, MediaPacket>
{
public:
	typedef std::function<void(int32_t, std::shared_ptr<MediaPacket>)> CompleteHandler;
	static std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> GetCandidates(bool hwaccels_enable, ov::String hwaccles_modules, std::shared_ptr<MediaTrack> track);
	static std::shared_ptr<TranscodeEncoder> Create(int32_t encoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> output_track, std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidates, CompleteHandler complete_handler);

public:
	TranscodeEncoder(info::Stream stream_info);
	~TranscodeEncoder() override;

	void SetEncoderId(int32_t encoder_id);
	virtual int GetSupportedFormat() const noexcept = 0;
	virtual cmn::BitstreamFormat GetBitstreamFormat() const noexcept = 0;
	bool Configure(std::shared_ptr<MediaTrack> output_track) override;
	void SendBuffer(std::shared_ptr<const MediaFrame> frame) override;
	void Complete(std::shared_ptr<MediaPacket> packet);
	virtual void CodecThread();
	virtual void Stop();
	void SetCompleteHandler(CompleteHandler complete_handler);
	
	std::shared_ptr<MediaTrack> &GetRefTrack();
	cmn::Timebase GetTimebase() const;
	
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
	cmn::BitstreamFormat _bitstream_format = cmn::BitstreamFormat::Unknown;
	cmn::PacketType _packet_type = cmn::PacketType::Unknown;

	info::Stream _stream_info;

	bool _kill_flag = false;
	std::thread _codec_thread;

	CompleteHandler _complete_handler;

	ov::PreciseTimer _force_keyframe_timer;

	// 0: no force keyframe,  > 0: force keyframe by sum of duration
	int64_t _force_keyframe_by_time_interval;
	// -1: force keyframe
	int64_t _accumulate_frame_duration;
};
