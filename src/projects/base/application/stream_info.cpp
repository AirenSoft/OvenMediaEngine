#include <random>
#include "stream_info.h"

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "StreamInfo"

using namespace MediaCommonType;

StreamInfo::StreamInfo()
{
	// 소스 타입
	// _input_source_type = kSourceTypeOrigin;
	
	// ID RANDOM 생성
	SetId(ov::Random::GenerateInteger());
}

StreamInfo::StreamInfo(uint32_t stream_id)
{
	_id = stream_id;
}

StreamInfo::StreamInfo(const StreamInfo &T)
{
	_id = T._id;
	_name = T._name;
	
	for (auto it=T._tracks.begin(); it!=T._tracks.end(); ++it)
	{
    	auto track = it->second;
    	auto new_track = std::make_shared<MediaTrack>(*track.get());
    	AddTrack(new_track);
    }
}

StreamInfo::~StreamInfo()
{

}


void StreamInfo::SetId(uint32_t id)
{
	_id = id;
}

uint32_t StreamInfo::GetId()
{
	return _id;
}

ov::String StreamInfo::GetName()
{
	return _name;
}

void StreamInfo::SetName(ov::String name)
{
	_name = name;
}

uint32_t StreamInfo::GetTrackCount()
{
	return _tracks.size();
}

bool StreamInfo::AddTrack(std::shared_ptr<MediaTrack> track)
{
	bool ret = _tracks.insert(std::make_pair(track->GetId(), track)).second;

	return ret;
}

std::shared_ptr<MediaTrack> StreamInfo::GetTrackAt(uint32_t index)
{
	uint32_t counter = 0;
	for (auto it = _tracks.begin(); it != _tracks.end(); ++it)
	{
		if (counter == index)
		{
			return it->second;
		}
		counter++;
    }

	return nullptr;
}

std::shared_ptr<MediaTrack> StreamInfo::GetTrack(uint32_t id)
{
	auto it = _tracks.find(id);

	if(it == _tracks.end())
	{
		return nullptr;
	}

    return it->second;
}


void StreamInfo::ShowInfo()
{
	ov::String out_str = "";

	out_str.AppendFormat("Stream Information / ");
	out_str.AppendFormat("id(%u), name(%s)", GetId(), GetName().CStr());
	for (auto it=_tracks.begin(); it!=_tracks.end(); ++it)
	{
    	auto track = it->second;

    	// TODO. CODEC_ID를 문자열로 변환하는 공통 함수를 만들어야함.
    	std::string codec_name;
    	switch(track->GetCodecId())
    	{
			case MediaCodecId::CODEC_ID_H264: codec_name = "avc"; break;
			case MediaCodecId::CODEC_ID_VP8: codec_name = "vp8"; break;
			case MediaCodecId::CODEC_ID_VP9: codec_name = "vp0"; break;
			case MediaCodecId::CODEC_ID_FLV: codec_name = "flv"; break;
			case MediaCodecId::CODEC_ID_AAC: codec_name = "aac"; break;
			case MediaCodecId::CODEC_ID_MP3: codec_name = "mp3"; break;
			case MediaCodecId::CODEC_ID_OPUS: codec_name = "opus"; break;
			case MediaCodecId::CODEC_ID_NONE:
			default:
			codec_name = "unknown";
			break;
		}

		if(track->GetMediaType() == MediaType::MEDIA_TYPE_VIDEO)	
		{
			out_str.AppendFormat(" / Video Track -> ");
			out_str.AppendFormat("track_id(%d) ", track->GetId()); 
			out_str.AppendFormat("codec(%d,%s) ", track->GetCodecId(), codec_name.c_str());
			out_str.AppendFormat("resolution(%dx%d) ", track->GetWidth(), track->GetHeight());
			out_str.AppendFormat("framerate(%.2ffps) ", track->GetFrameRate());
		}
		else if(track->GetMediaType() == MediaType::MEDIA_TYPE_AUDIO) 
		{
			out_str.AppendFormat(" / Audio Track -> ");
			out_str.AppendFormat("track_id(%d) ", track->GetId());
			out_str.AppendFormat("codec(%d,%s) ",  track->GetCodecId(), codec_name.c_str());
			out_str.AppendFormat("samplerate(%d) ", track->GetSampleRate());
			out_str.AppendFormat("format(%s, %d) ", track->GetSample().GetName(), track->GetSample().GetSampleSize());
			out_str.AppendFormat("channel(%s, %d) ", track->GetChannel().GetName(), track->GetChannel().GetCounts());
		}

		out_str.AppendFormat("timebase(%d/%d)", track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen());
    }

    logi("StreamInfo", "%s", out_str.CStr());
}