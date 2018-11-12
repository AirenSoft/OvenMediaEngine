//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application_private.h"

#include "stream.h"

namespace info
{
	bool Stream::AddTrack(std::shared_ptr<MediaTrack> track)
	{
		return _tracks.insert(std::make_pair(track->GetId(), track)).second;
	}

	const std::shared_ptr<MediaTrack> Stream::GetTrack(int32_t id) const
	{
		auto item = _tracks.find(id);

		if(item == _tracks.end())
		{
			return nullptr;
		}

		return item->second;
	}

	const std::map<int32_t, std::shared_ptr<MediaTrack>> &Stream::GetTracks() const
	{
		return _tracks;
	}

	void Stream::ShowInfo()
	{
		ov::String out_str = ov::String::FormatString("Stream Information / id(%u), name(%s)", GetId(), GetName().CStr());

		for(auto it = _tracks.begin(); it != _tracks.end(); ++it)
		{
			auto track = it->second;

			// TODO. CODEC_ID를 문자열로 변환하는 공통 함수를 만들어야함.
			ov::String codec_name;

			switch(track->GetCodecId())
			{
				case common::MediaCodecId::H264:
					codec_name = "avc";
					break;

				case common::MediaCodecId::Vp8:
					codec_name = "vp8";
					break;

				case common::MediaCodecId::Vp9:
					codec_name = "vp0";
					break;

				case common::MediaCodecId::Flv:
					codec_name = "flv";
					break;

				case common::MediaCodecId::Aac:
					codec_name = "aac";
					break;

				case common::MediaCodecId::Mp3:
					codec_name = "mp3";
					break;

				case common::MediaCodecId::Opus:
					codec_name = "opus";
					break;

				case common::MediaCodecId::None:
				default:
					codec_name = "unknown";
					break;
			}

			switch(track->GetMediaType())
			{
				case common::MediaType::Video:
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

				case common::MediaType::Audio:
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
}