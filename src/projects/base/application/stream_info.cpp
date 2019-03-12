#include <random>
#include "stream_info.h"

#define OV_LOG_TAG "StreamInfo"

using namespace common;

StreamInfo::StreamInfo()
{
	// 소스 타입
	// _input_source_type = kSourceTypeOrigin;

	// ID RANDOM 생성
	SetId(ov::Random::GenerateUInt32());
}

StreamInfo::StreamInfo(uint32_t stream_id)
{
	_id = stream_id;
}

StreamInfo::StreamInfo(const StreamInfo &stream_info)
{
	_id = stream_info._id;
	_name = stream_info._name;

	for(auto &track : stream_info._tracks)
	{
		auto new_track = std::make_shared<MediaTrack>(*(track.second.get()));
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

uint32_t StreamInfo::GetId() const
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

bool StreamInfo::AddTrack(std::shared_ptr<MediaTrack> track)
{
	return _tracks.insert(std::make_pair(track->GetId(), track)).second;
}

const std::shared_ptr<MediaTrack> StreamInfo::GetTrack(int32_t id) const
{
	auto item = _tracks.find(id);

	if(item == _tracks.end())
	{
		return nullptr;
	}

	return item->second;
}

const std::map<int32_t, std::shared_ptr<MediaTrack>> &StreamInfo::GetTracks() const
{
	return _tracks;
}

void StreamInfo::ShowInfo()
{
	ov::String out_str = ov::String::FormatString("Stream Information / id(%u), name(%s)", GetId(), GetName().CStr());

	for(auto it = _tracks.begin(); it != _tracks.end(); ++it)
	{
		auto track = it->second;

		// TODO. CODEC_ID를 문자열로 변환하는 공통 함수를 만들어야함.
		ov::String codec_name;

		switch(track->GetCodecId())
		{
			case MediaCodecId::H264:
				codec_name = "avc";
				break;

			case MediaCodecId::Vp8:
				codec_name = "vp8";
				break;

			case MediaCodecId::Vp9:
				codec_name = "vp0";
				break;

			case MediaCodecId::Flv:
				codec_name = "flv";
				break;

			case MediaCodecId::Aac:
				codec_name = "aac";
				break;

			case MediaCodecId::Mp3:
				codec_name = "mp3";
				break;

			case MediaCodecId::Opus:
				codec_name = "opus";
				break;

			case MediaCodecId::None:
			default:
				codec_name = "unknown";
				break;
		}

		switch(track->GetMediaType())
		{
			case MediaType::Video:
				out_str.AppendFormat(
					" / Video Track -> "
					"track_id(%d) "
					"codec(%d,%s) "
					"resolution(%dx%d) "
					"framerate(%.2ffps) ",
					track->GetId(),
					track->GetCodecId(), codec_name.CStr(),
					track->GetWidth(), track->GetHeight(),
					track->GetFrameRate()
				);
				break;

			case MediaType::Audio:
				out_str.AppendFormat(
					" / Audio Track -> "
					"track_id(%d) "
					"codec(%d, %s) "
					"samplerate(%d) "
					"format(%s, %d) "
					"channel(%s, %d) ",
					track->GetId(),
					track->GetCodecId(), codec_name.CStr(),
					track->GetSampleRate(),
					track->GetSample().GetName(), track->GetSample().GetSampleSize(),
					track->GetChannel().GetName(), track->GetChannel().GetCounts()
				);
				break;

			default:
				break;
		}

		out_str.AppendFormat("timebase(%s)", track->GetTimeBase().ToString().CStr());
	}

	logti("%s", out_str.CStr());
}