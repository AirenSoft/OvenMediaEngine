#include "media_track.h"

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MediaTrack"

MediaTrack::MediaTrack() :
	_id(0),
	_codec_id(MediaCodecId::CODEC_ID_NONE),
	_start_frame_time(0),
	_last_frame_time(0)
{
	
}

MediaTrack::MediaTrack(const MediaTrack &T)
{
	_id = T._id;
	_media_type = T._media_type;
	_codec_id = T._codec_id;

	// 비디오
	_framerate = T._framerate;
	_width = T._width;
	_height = T._height;

	// 오디오
	// _sample_rate = T._sample_rate;
	_sample = T._sample;
	_channel_layout = T._channel_layout;

	_time_base = T._time_base;

	_start_frame_time = 0;
	_last_frame_time = 0;
}

MediaTrack::~MediaTrack()
{

}

void MediaTrack::SetId(uint32_t id)
{
	_id = id;
}

uint32_t MediaTrack::GetId()
{
	return _id;
}

void MediaTrack::SetMediaType(MediaType type)
{
	_media_type = type;
}

MediaType MediaTrack::GetMediaType()
{
	return _media_type;
}

void MediaTrack::SetCodecId(MediaCodecId id)
{
	_codec_id = id;
}

MediaCodecId MediaTrack::GetCodecId()
{
	return _codec_id;
}

Timebase& MediaTrack::GetTimeBase()
{
	return _time_base;
}

void MediaTrack::SetTimeBase(int32_t num, int32_t den)
{
	_time_base.Set(num, den);
}


void MediaTrack::SetStartFrameTime(int64_t time)
{
	_start_frame_time = time;
}

int64_t MediaTrack::GetStartFrameTime()
{
	return _start_frame_time;
}

void MediaTrack::SetLastFrameTime(int64_t time)
{
	_last_frame_time = time;
}

int64_t MediaTrack::GetLastFrameTime()
{
	return _last_frame_time;
}

void VideoTrack::SetFrameRate(double framerate)
{
	_framerate = framerate;
}

double VideoTrack::GetFrameRate()
{
	return _framerate;
}

void VideoTrack::SetWidth(uint32_t width)
{
	_width = width;
}

uint32_t VideoTrack::GetWidth()
{
	return _width;
}

void VideoTrack::SetHeight(uint32_t height)
{
	_height = height;
}

uint32_t VideoTrack::GetHeight()
{
	return _height;
}

void AudioTrack::SetSampleRate(int32_t sample_rate)
{
	_sample.SetRate((AudioSample::Rate)sample_rate);
}

int32_t AudioTrack::GetSampleRate()
{
	return (int32_t)_sample.GetRate();
}

AudioSample& AudioTrack::GetSample()
{
	return _sample;
}

AudioSample::Format AudioTrack::GetSampleFormat()
{
	return _sample.GetFormat();
}

AudioChannel::Layout AudioTrack::GetChannelLayout()
{
	return _channel_layout.GetLayout();
}

AudioChannel& AudioTrack::GetChannel()
{
	return _channel_layout;	
}
