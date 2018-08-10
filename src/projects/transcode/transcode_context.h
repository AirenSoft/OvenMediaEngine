//==============================================================================
//
//  TranscodeContext
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>

#include <base/ovlibrary/ovlibrary.h>
#include "base/media_route/media_type.h"

//TODO: 현 버전에서 지원하는 트랜스코딩은 1 Video + 1 Audio 방식으로 고정한다.
// TranscodeContext 데이터를 코덱 및 필터에 공용으로 사용된다
using namespace MediaCommonType;

class TranscodeContext
{
public:
	TranscodeContext();
	~TranscodeContext();

	// 비디오 트랜스코딩 옵션
public:
	void SetVideoCodecId(MediaCodecId val);
	MediaCodecId GetVideoCodecId();

	void SetVideoBitrate(int32_t val);
	int32_t GetVideoBitrate();

	void SetVideoWidth(uint32_t val);
	void SetVideoHeight(uint32_t val);

	uint32_t GetVideoWidth();
	uint32_t GetVideoHeight();

	void SetFrameRate(float val);
	float GetFrameRate();

	Timebase &GetVideoTimeBase();
	void SetVideoTimeBase(int32_t num, int32_t den);

	void SetGOP(int32_t val);
	int32_t GetGOP();

	MediaCodecId _video_codec_id;

	// Bitrate
	int32_t _video_bitrate;

	// Resolution
	uint32_t _video_width;
	uint32_t _video_height;

	// Frame Rate
	float _video_frame_rate;

	// GOP : Group Of Picture
	int32_t _video_gop;

	// 비디오 타입 베이스
	Timebase _video_time_base;



	// 오디오 트랜스코딩 옵션
public:

	void SetAudioCodecId(MediaCodecId val);
	MediaCodecId GetAudioCodecId();

	void SetAudioBitrate(int32_t val);
	int32_t GetAudioBitrate();

	void SetAudioSampleRate(int32_t val);
	int32_t GetAudioSampleRate();

	Timebase &GetAudioTimeBase();

	void SetAudioTimeBase(int32_t num, int32_t den);

	void SetAudioSampleForamt(AudioSample::Format val);

	void SetAudioChannelLayout(AudioChannel::Layout val);

	MediaCodecId _audio_codec_id;

	// 오디오 비트레이트
	int32_t _audio_bitrate;

	// 오디오 샘플
	AudioSample _audio_sample;

	// 오디오 채널
	AudioChannel _audio_channel;

	// 오디오 타임베이스
	Timebase _audio_time_base;
};

