//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_track.h"
#include "audio_track.h"

class MediaTrack : public VideoTrack, public AudioTrack
{
public:
	MediaTrack();
	MediaTrack(const MediaTrack &media_track);
	~MediaTrack();

	void SetId(uint32_t id);
	uint32_t GetId();

// private:
	// 트랙 아이디
	uint32_t _id;

public:
	// 비디오 오디오 설정
	void SetMediaType(MediaCommonType::MediaType type);
	MediaCommonType::MediaType GetMediaType();

	// 코덱 설정
	void SetCodecId(MediaCommonType::MediaCodecId id);
	MediaCommonType::MediaCodecId GetCodecId();

	// 타임베이스 설정
	MediaCommonType::Timebase &GetTimeBase();
	void SetTimeBase(int32_t num, int32_t den);

	void SetStartFrameTime(int64_t time);
	int64_t GetStartFrameTime();

	void SetLastFrameTime(int64_t time);
	int64_t GetLastFrameTime();

// private:

	// 코덱 아이디
	MediaCommonType::MediaCodecId _codec_id;

	// 트랙 타입
	MediaCommonType::MediaType _media_type;

	// 타입 베이스
	MediaCommonType::Timebase _time_base;

	// 시작 프레임(패킷) 시간
	int64_t _start_frame_time;

	// 마지막 프레임(패킷) 시간
	int64_t _last_frame_time;
};