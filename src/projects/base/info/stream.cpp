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

using namespace cmn;

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
		_source_url = stream._source_url;
		_created_time = stream._created_time;
		_published_time = stream._published_time;
		_app_info = stream._app_info;
		_origin_stream = stream._origin_stream;

		_tracks = stream._tracks;
		_video_tracks = stream._video_tracks;
		_audio_tracks = stream._audio_tracks;

		_track_group_map = stream._track_group_map;

		_playlists = stream._playlists;
		_representation_type = stream._representation_type;
	}

	Stream::Stream(StreamSourceType source)
	{
		_source_type = source;
		_created_time = std::chrono::system_clock::now();
	}

	Stream::~Stream()
	{
		logd("DEBUG", "Stream (%s / %s) Destroyed", GetName().CStr(), GetUUID().CStr());
	}

	bool Stream::operator==(const Stream &stream_info) const
	{
		if (_id == stream_info._id && *_app_info == *stream_info._app_info)
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

	ov::String Stream::GetUri()
	{
		// #vhost name#appname/stream name
		ov::String vhost_app_name = _app_info != nullptr ? _app_info->GetName().CStr() : "Unknown";
		return ov::String::FormatString("%s/%s", vhost_app_name.CStr(), GetName().CStr());
	}

	void Stream::SetMsid(uint32_t msid)
	{
		_msid = msid;
	}

	uint32_t Stream::GetMsid()
	{
		return _msid;
	}

	ov::String Stream::GetUUID() const
	{
		if (_app_info == nullptr)
		{
			return "";
		}

		return ov::String::FormatString("%s/%s/%s", _app_info->GetUUID().CStr(), GetName().CStr(), IsInputStream() ? "i" : "o");
	}

	ov::String Stream::GetName() const
	{
		return _name;
	}

	void Stream::SetName(ov::String name)
	{
		_name = std::move(name);
	}

	ov::String Stream::GetMediaSource() const
	{
		return _source_url;
	}
	void Stream::SetMediaSource(ov::String url)
	{
		_source_url = url;
	}

	bool Stream::IsInputStream() const
	{
		return IsOutputStream() == false;
	}

	bool Stream::IsOutputStream() const
	{
		return GetSourceType() == StreamSourceType::Transcoder || GetLinkedInputStream() != nullptr;
	}

	void Stream::LinkInputStream(const std::shared_ptr<Stream> &stream)
	{
		_origin_stream = stream;
	}

	const std::shared_ptr<Stream> Stream::GetLinkedInputStream() const
	{
		return _origin_stream;
	}

	// Only used in OVT provider
	void Stream::SetOriginStreamUUID(const ov::String &uuid)
	{
		_origin_stream_uuid = uuid;
	}

	ov::String Stream::GetOriginStreamUUID() const
	{
		return _origin_stream_uuid;
	}

	const std::chrono::system_clock::time_point &Stream::GetInputStreamCreatedTime() const
	{
		if (GetLinkedInputStream() != nullptr)
		{
			return GetLinkedInputStream()->GetCreatedTime();
		}

		return GetCreatedTime();
	}

	const std::chrono::system_clock::time_point &Stream::GetCreatedTime() const
	{
		return _created_time;
	}

	void Stream::SetPublishedTimeNow()
	{
		_published_time = std::chrono::system_clock::now();
	}

	const std::chrono::system_clock::time_point &Stream::GetPublishedTime() const
	{
		return _published_time;
	}

	const std::chrono::system_clock::time_point &Stream::GetInputStreamPublishedTime() const
	{
		if (GetLinkedInputStream() != nullptr)
		{
			return GetLinkedInputStream()->GetPublishedTime();
		}

		return GetPublishedTime();
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

	StreamRepresentationType Stream::GetRepresentationType() const
	{
		return _representation_type;
	}

	void Stream::SetRepresentationType(const StreamRepresentationType &type)
	{
		_representation_type = type;
	}

	uint32_t Stream::IssueUniqueTrackId()
	{
		static std::atomic<uint32_t> last_issued_track_id(1);
		return last_issued_track_id++;
	}

	bool Stream::AddTrack(const std::shared_ptr<MediaTrack> &track)
	{
		auto item = _tracks.find(track->GetId());
		if (item != _tracks.end())
		{
			return false;
		}

		_tracks.emplace(track->GetId(), track);

		if (track->GetMediaType() == cmn::MediaType::Video)
		{
			_video_tracks.push_back(track);
		}
		else if (track->GetMediaType() == cmn::MediaType::Audio)
		{
			_audio_tracks.push_back(track);
		}

		// Add to group
		auto group_it = _track_group_map.find(track->GetVariantName());
		if (group_it == _track_group_map.end())
		{
			auto group = std::make_shared<MediaTrackGroup>(track->GetVariantName());
			group->AddTrack(track);
			_track_group_map.emplace(track->GetVariantName(), group);
		}
		else
		{
			auto group = group_it->second;
			group->AddTrack(track);
		}

		return true;
	}

	// If track is not exist, add track or update track
	bool Stream::UpdateTrack(const std::shared_ptr<MediaTrack> &track)
	{
		auto ex_track = GetTrack(track->GetId());
		if (ex_track == nullptr)
		{
			return AddTrack(track);
		}

		return ex_track->Update(*track);
	}

	bool Stream::RemoveTrack(uint32_t id)
	{
		auto track = GetTrack(id);
		if (track == nullptr)
		{
			return true;
		}

		_tracks.erase(id);

		// Remove from vectors
		if (track->GetMediaType() == cmn::MediaType::Video)
		{
			for (auto it = _video_tracks.begin(); it != _video_tracks.end(); ++it)
			{
				if ((*it)->GetId() == id)
				{
					_video_tracks.erase(it);
					break;
				}
			}
		}
		else if (track->GetMediaType() == cmn::MediaType::Audio)
		{
			for (auto it = _audio_tracks.begin(); it != _audio_tracks.end(); ++it)
			{
				if ((*it)->GetId() == id)
				{
					_audio_tracks.erase(it);
					break;
				}
			}
		}

		// Remove from group
		auto group_it = _track_group_map.find(track->GetVariantName());
		if (group_it != _track_group_map.end())
		{
			auto group = group_it->second;
			group->RemoveTrack(id);
		}

		return true;
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

	const std::shared_ptr<MediaTrackGroup> Stream::GetMediaTrackGroup(const ov::String &group_name) const
	{
		auto item = _track_group_map.find(group_name);
		if (item == _track_group_map.end())
		{
			return nullptr;
		}

		return item->second;
	}

	const std::map<ov::String, std::shared_ptr<MediaTrackGroup>> &Stream::GetMediaTrackGroups() const
	{
		return _track_group_map;
	}

	uint32_t Stream::GetMediaTrackCount(const cmn::MediaType &type) const
	{
		if (type == cmn::MediaType::Video)
		{
			return _video_tracks.size();
		}
		else if (type == cmn::MediaType::Audio)
		{
			return _audio_tracks.size();
		}

		return 0;
	}
	
	// start from 0
	const std::shared_ptr<MediaTrack> Stream::GetMediaTrackByOrder(const cmn::MediaType &type, uint32_t order) const
	{
		if (type == cmn::MediaType::Video)
		{
			if (order >= _video_tracks.size())
			{
				return nullptr;
			}

			return _video_tracks[order];
		}
		else if (type == cmn::MediaType::Audio)
		{
			if (order >= _audio_tracks.size())
			{
				return nullptr;
			}

			return _audio_tracks[order];
		}

		return nullptr;
	}

	// Get Track by variant name
	const std::shared_ptr<MediaTrack> Stream::GetFirstTrackByVariant(const ov::String &variant_name) const
	{
		auto group = GetMediaTrackGroup(variant_name);
		if (group == nullptr || group->GetTrackCount() == 0)
		{
			return nullptr;
		}

		return group->GetTrack(0);
	}

	const std::shared_ptr<MediaTrack> Stream::GetFirstTrackByType(const cmn::MediaType &type) const
	{
		for (auto &item : _tracks)
		{
			if (item.second->GetMediaType() == type)
			{
				return item.second;
			}
		}

		return nullptr;
	}

	const std::map<int32_t, std::shared_ptr<MediaTrack>> &Stream::GetTracks() const
	{
		return _tracks;
	}

	bool Stream::AddPlaylist(const std::shared_ptr<Playlist> &playlist)
	{
		auto result = _playlists.emplace(playlist->GetFileName(), playlist);
		return result.second;
	}

	std::shared_ptr<const Playlist> Stream::GetPlaylist(const ov::String &file_name) const
	{
		auto item = _playlists.find(file_name);
		if (item == _playlists.end())
		{
			return nullptr;
		}

		return item->second;
	}

	const std::map<ov::String, std::shared_ptr<Playlist>> &Stream::GetPlaylists() const
	{
		return _playlists;
	}

	const char *Stream::GetApplicationName()
	{
		if (_app_info == nullptr)
		{
			return "Unknown";
		}

		return _app_info->GetName().CStr();
	}

	ov::String Stream::GetInfoString()
	{
		ov::String out_str = ov::String::FormatString("\n[Stream Info]\nid(%u), msid(%u), output(%s), SourceType(%s), RepresentationType(%s), Created Time (%s) UUID(%s)\n",
													  GetId(), GetMsid(), GetName().CStr(), ::StringFromStreamSourceType(_source_type).CStr(), ::StringFromStreamRepresentationType(_representation_type).CStr(),
													  ov::Converter::ToString(_created_time).CStr(), GetUUID().CStr());
		if (GetLinkedInputStream() != nullptr)
		{
			out_str.AppendFormat("\t>> Origin Stream Info\n\tid(%u), output(%s), SourceType(%s), Created Time (%s)\n",
								 GetLinkedInputStream()->GetId(), GetLinkedInputStream()->GetName().CStr(), ::StringFromStreamSourceType(GetLinkedInputStream()->GetSourceType()).CStr(),
								 ov::Converter::ToString(GetLinkedInputStream()->GetCreatedTime()).CStr());
		}

		if (GetOriginStreamUUID().IsEmpty() == false)
		{
			out_str.AppendFormat("\t>> Origin Stream UUID : %s\n", GetOriginStreamUUID().CStr());
		}

		for (auto it = _tracks.begin(); it != _tracks.end(); ++it)
		{
			auto track = it->second;

			out_str.AppendFormat("\n\t%s", track->GetInfoString().CStr());
		}

		return out_str;
	}

	void Stream::ShowInfo()
	{
		logi("Monitor", "%s", GetInfoString().CStr());
	}
}  // namespace info