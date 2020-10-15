//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stream.h"
#include <random>
#include "application.h"

#define OV_LOG_TAG "Stream"

using namespace common;

namespace info
{
	Stream::Stream(const info::Application &app_info, StreamSourceType source)
	{
		_app_info = std::make_shared<info::Application>(app_info);

		// ID RANDOM 생성
		SetId(ov::Random::GenerateUInt32() - 1);

		_created_time = std::chrono::system_clock::now();
		_source_type = source;
	}

	Stream::Stream(const info::Application &app_info, info::stream_id_t stream_id, StreamSourceType source)
	{
		_app_info = std::make_shared<info::Application>(app_info);

		_id = stream_id;
		_created_time = std::chrono::system_clock::now();
		_source_type = source;
	}

	Stream::Stream(const Stream &stream)
	{
		_id = stream._id;
		_name = stream._name;
		_source_type = stream._source_type;
		_created_time = stream._created_time;
		_app_info = stream._app_info;
		_origin_stream = stream._origin_stream;

		for (auto &track : stream._tracks)
		{
			AddTrack(track.second);
		}
	}

	Stream::Stream(StreamSourceType source)
	{
		_source_type = source;
	}

	Stream::~Stream()
	{
	}

	bool Stream::operator==(const Stream &stream_info) const
	{
		if(_id == stream_info._id && *_app_info == *stream_info._app_info)
		{
			return true;
		}

		return false;
	}

	void Stream::SetId(info::stream_id_t id)
	{
		_id = id;
	}

	info::stream_id_t Stream::GetId() const
	{
		return _id;
	}

	ov::String Stream::GetName() const 
	{
		return _name;
	}

	void Stream::SetName(ov::String name)
	{
		_name = name;
	}

	ov::String Stream::GetMediaSource() const
	{
		return _source_url;
	}
	void Stream::SetMediaSource(ov::String url)
	{
		_source_url = url;
	}

	void Stream::SetOriginStream(const std::shared_ptr<Stream> &stream)
	{
		_origin_stream = stream;
	}

	const std::shared_ptr<Stream> Stream::GetOriginStream() const
	{
		return _origin_stream;
	}

	const std::chrono::system_clock::time_point& Stream::GetCreatedTime() const
	{
		return _created_time;
	}

	uint32_t Stream::GetUptimeSec()
	{
		auto current = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::seconds>(current - GetCreatedTime()).count();
	}

	StreamSourceType Stream::GetSourceType() const
	{
		return _source_type;
	}

	bool Stream::AddTrack(std::shared_ptr<MediaTrack> track)
	{
		return _tracks.insert(std::make_pair(track->GetId(), track)).second;
	}

	const std::shared_ptr<MediaTrack> Stream::GetTrack(int32_t id) const
	{
		auto item = _tracks.find(id);
		if (item == _tracks.end())
		{
			return nullptr;
		}

		return item->second;
	}

	const std::map<int32_t, std::shared_ptr<MediaTrack>> &Stream::GetTracks() const
	{
		return _tracks;
	}

	const char* Stream::GetApplicationName()
	{
		if(_app_info == nullptr)
		{
			return "Unknown";
		}

		return _app_info->GetName().CStr();
	}


	ov::String Stream::GetInfoString()
	{
		ov::String out_str = ov::String::FormatString("\n[Stream Info]\nid(%u), name(%s), SourceType(%s), Created Time (%s)\n", 														
														GetId(), GetName().CStr(), ov::Converter::ToString(_source_type).CStr(),
														ov::Converter::ToString(_created_time).CStr());
		if(GetOriginStream() != nullptr)
		{
			out_str.AppendFormat("\t>> Origin Stream Info\n\tid(%u), name(%s), SourceType(%s), Created Time (%s)\n",
				GetOriginStream()->GetId(), GetOriginStream()->GetName().CStr(), ov::Converter::ToString(GetOriginStream()->GetSourceType()).CStr(),
														ov::Converter::ToString(GetOriginStream()->GetCreatedTime()).CStr());
		}

		for (auto it = _tracks.begin(); it != _tracks.end(); ++it)
		{
			auto track = it->second;

			switch (track->GetMediaType())
			{
				case MediaType::Video:
					out_str.AppendFormat(
						"\n\tVideo Track #%d: "
						"Bypass(%s) "
						"Bitrate(%s) "
						"codec(%d, %s) "
						"resolution(%dx%d) "
						"framerate(%.2ffps) ",
						track->GetId(),
						track->IsBypass() ? "true" : "false",
						ov::Converter::BitToString(track->GetBitrate()).CStr(),
						track->GetCodecId(), ov::Converter::ToString(track->GetCodecId()).CStr(),
						track->GetWidth(), track->GetHeight(),
						track->GetFrameRate());
					break;

				case MediaType::Audio:
					out_str.AppendFormat(
						"\n\tAudio Track #%d: "
						"Bypass(%s) "
						"Bitrate(%s) "
						"codec(%d, %s) "
						"samplerate(%s) "
						"format(%s, %d) "
						"channel(%s, %d) ",
						track->GetId(),
						track->IsBypass() ? "true" : "false",
						ov::Converter::BitToString(track->GetBitrate()).CStr(),
						track->GetCodecId(), ov::Converter::ToString(track->GetCodecId()).CStr(),
						ov::Converter::ToSiString(track->GetSampleRate(), 1).CStr(),
						track->GetSample().GetName(), track->GetSample().GetSampleSize() * 8,
						track->GetChannel().GetName(), track->GetChannel().GetCounts());
					break;

				default:
					break;
			}

			out_str.AppendFormat("timebase(%s)", track->GetTimeBase().ToString().CStr());
		}

		return out_str;
	}

	void Stream::ShowInfo()
	{
		logi("Monitor", "%s", GetInfoString().CStr());
	}
}  // namespace info