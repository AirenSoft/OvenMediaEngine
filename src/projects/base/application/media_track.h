#pragma once

#include "base/common_types.h"

using namespace MediaCommonType;

class VideoTrack 
{
public:
	void SetFrameRate(double framerate);
	double GetFrameRate();

	void SetWidth(uint32_t width);
	uint32_t GetWidth();

	void SetHeight(uint32_t height);
	uint32_t GetHeight();

// private:
	// 프레임 레이트
	double _framerate;

	// 가로
	uint32_t _width;

	// 세로
	uint32_t _height;
};

class AudioTrack 
{
public:
	void SetSampleRate(int32_t samplerate);
	int32_t GetSampleRate();

	AudioSample::Format GetSampleFormat();

	AudioChannel::Layout GetChannelLayout();

	AudioSample& GetSample();

	AudioChannel& GetChannel();
	
// private:
	// 샘플 포맷, 샘플 레이트
	AudioSample _sample;

	// 채널 레이아웃
	AudioChannel _channel_layout;

};

class MediaTrack : public VideoTrack, public AudioTrack
{
public:
	MediaTrack();
	MediaTrack(const MediaTrack &T);
	~MediaTrack();
	
	void 		SetId(uint32_t id);
	uint32_t 	GetId();

// private:
	// 트랙 아이디
	uint32_t 	_id;

public:
	// 비디오 오디오 설정
	void SetMediaType(MediaType type);
	MediaType GetMediaType();

	// 코덱 설정
	void SetCodecId(MediaCodecId id);
	MediaCodecId GetCodecId();

	// 타임베이스 설정
	Timebase& GetTimeBase();
	void SetTimeBase(int32_t num, int32_t den);
	
	void SetStartFrameTime(int64_t time);
	int64_t GetStartFrameTime();

	void SetLastFrameTime(int64_t time);
	int64_t GetLastFrameTime();

// private:

	// 코덱 아이디
	MediaCodecId _codec_id;

	// 트랙 타입
	MediaType _media_type;

	// 타입 베이스
	Timebase _time_base;

	// 시작 프레임(패킷) 시간
	int64_t _start_frame_time;

	// 마지막 프레임(패킷) 시간
	int64_t _last_frame_time;
};