//==============================================================================
//
//  Multiplex
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "multiplex_profile.h"
#include "multiplex_private.h"
#include <base/ovlibrary/files.h>

namespace pvd
{
	std::tuple<std::shared_ptr<MultiplexProfile>, ov::String> MultiplexProfile::CreateFromXMLFile(const ov::String &file_path)
	{
		auto multiplex = std::make_shared<MultiplexProfile>();
		if (multiplex->LoadFromXMLFile(file_path) == false)
		{
			return {nullptr, multiplex->GetLastError()};
		}

		return {multiplex, "success"};
	}

	std::tuple<std::shared_ptr<MultiplexProfile>, ov::String> MultiplexProfile::CreateFromJsonObject(const Json::Value &object)
	{
		auto multiplex = std::make_shared<MultiplexProfile>();
		if (multiplex->LoadFromJsonObject(object) == false)
		{
			return {nullptr, multiplex->GetLastError()};
		}

		return {multiplex, "success"};
	}

	std::shared_ptr<MultiplexProfile> MultiplexProfile::Clone() const
	{
		auto multiplex = std::make_shared<MultiplexProfile>();

		Json::Value root_object;
		if (ToJsonObject(root_object) != CommonErrorCode::SUCCESS)
		{
			return nullptr;
		}

		multiplex->LoadFromJsonObject(root_object);

		return multiplex;
	}

	bool MultiplexProfile::operator==(const MultiplexProfile &other) const
	{
		if (_output_stream_name != other._output_stream_name)
		{
			return false;
		}

		if (_source_streams.size() != other._source_streams.size())
		{
			return false;
		}

		if (_playlists.size() != other._playlists.size())
		{
			return false;
		}

		for (size_t i = 0; i < _playlists.size(); i++)
		{
			if (*_playlists[i] != *other._playlists[i])
			{
				return false;
			}
		}

		for (size_t i = 0; i < _source_streams.size(); i++)
		{
			if (*_source_streams[i] != *other._source_streams[i])
			{
				return false;
			}
		}

		return true;
	}

	bool MultiplexProfile::LoadFromJsonObject(const Json::Value &object)
	{
		return true;
	}

	bool MultiplexProfile::LoadFromXMLFile(const ov::String &file_path)
	{
		pugi::xml_document xml_doc;
		auto load_result = xml_doc.load_file(file_path.CStr());
		if (!load_result)
		{
			_last_error = ov::String::FormatString("Failed to load schedule file: %s", load_result.description());
			return false;
		}

		auto root_node = xml_doc.child("Multiplex");
		if (!root_node)
		{
			_last_error = "Failed to find root node";
			return false;
		}

		if (ReadOutputStreamNode(root_node) == false)
		{
			return false;
		}

		if (ReadSourceStreamsNode(root_node) == false)
		{
			return false;
		}

		if (ReadPlaylistsNode(root_node) == false)
		{
			return false;
		}

		return true;
	}

	bool MultiplexProfile::ReadOutputStreamNode(const pugi::xml_node &root_node)
	{
		auto output_stream_node = root_node.child("OutputStream");
		if (!output_stream_node)
		{
			_last_error = "Failed to find OutputStream node";
			return false;
		}

		auto name = output_stream_node.child("Name");
		if (!name)
		{
			_last_error = "Failed to find Name node";
			return false;
		}

		_output_stream_name = name.text().as_string();

		return true;
	}

	bool MultiplexProfile::ReadPlaylistsNode(const pugi::xml_node &root_node)
	{
		auto playlists_node = root_node.child("Playlists");

		for (auto playlist_node = playlists_node.child("Playlist"); playlist_node; playlist_node = playlist_node.next_sibling("Playlist"))
		{
			auto playlist_name_node = playlist_node.child("Name");
			if (!playlist_name_node)
			{
				_last_error = "Failed to find Playlists/Playlist/Name node";
				return false;
			}

			auto file_name_node = playlist_node.child("FileName");
			if (!file_name_node)
			{
				_last_error = "Failed to find Playlists/Playlist/FileName node";
				return false;
			}

			auto playlist = std::make_shared<info::Playlist>(playlist_name_node.text().as_string(), file_name_node.text().as_string());

			// Options
			auto options_node = playlist_node.child("Options");
			if (options_node)
			{
				auto webrtc_auto_abr = options_node.child("WebRtcAutoAbr");
				if (webrtc_auto_abr)
				{
					playlist->SetWebRtcAutoAbr(webrtc_auto_abr.text().as_bool());
				}

				auto hls_chunklist_path_depth = options_node.child("HlsChunklistPathDepth");
				if (hls_chunklist_path_depth)
				{
					playlist->SetHlsChunklistPathDepth(hls_chunklist_path_depth.text().as_int());
				}
			}

			// Rendition
			for (auto rendition_node = playlist_node.child("Rendition"); rendition_node; rendition_node = rendition_node.next_sibling("Rendition"))
			{
				ov::String rendition_name;
				ov::String video_variant_name;
				ov::String audio_variant_name;

				auto rendition_name_node = rendition_node.child("Name");
				if (!rendition_name_node)
				{
					_last_error = "Failed to find Playlists/Playlist/Rendition/Name node";
					return false;
				}

				rendition_name = rendition_name_node.text().as_string();

				auto video_variant_name_node = rendition_node.child("Video");
				if (video_variant_name_node)
				{
					video_variant_name = video_variant_name_node.text().as_string();

					// Check if video variant name is exist in SourceStream/TrackMap/NewTrackName
					if (_new_track_names.find(video_variant_name) == _new_track_names.end())
					{
						_last_error = ov::String::FormatString("Failed to find video variant name %s in SourceStream/TrackMap/NewTrackName", video_variant_name.CStr());
						return false;
					}
				}

				auto audio_variant_name_node = rendition_node.child("Audio");
				if (audio_variant_name_node)
				{
					audio_variant_name = audio_variant_name_node.text().as_string();

					// Check if audio variant name is exist in SourceStream/TrackMap/NewTrackName
					if (_new_track_names.find(audio_variant_name) == _new_track_names.end())
					{
						_last_error = ov::String::FormatString("Failed to find audio variant name %s in SourceStream/TrackMap/NewTrackName", audio_variant_name.CStr());
						return false;
					}
				}

				auto rendition = std::make_shared<info::Rendition>(rendition_name, video_variant_name, audio_variant_name);
				playlist->AddRendition(rendition);
			}

			_playlists.push_back(playlist);
		}

		return true;
	}

	bool MultiplexProfile::ReadSourceStreamsNode(const pugi::xml_node &root_node)
	{
		auto source_streams_node = root_node.child("SourceStreams");

		for (auto source_stream_node = source_streams_node.child("SourceStream"); source_stream_node; source_stream_node = source_stream_node.next_sibling("SourceStream"))
		{
			auto name_node = source_stream_node.child("Name");
			if (!name_node)
			{
				_last_error = "Failed to find SourceStreams/SourceStream/Name node";
				return false;
			}

			auto url_node = source_stream_node.child("Url");
			if (!url_node)
			{
				_last_error = "Failed to find SourceStreams/SourceStream/Url node";
				return false;
			}

			auto source_stream = std::make_shared<SourceStream>();
			source_stream->SetName(name_node.text().as_string());
			ov::String url_str = url_node.text().as_string();
			if (source_stream->SetUrl(url_str) == false)
			{
				_last_error = ov::String::FormatString("Failed to parse url: %s", url_str.CStr());
				return false;
			}
			
			// TrackMap
			auto track_map_node = source_stream_node.child("TrackMap");

			// Track
			for (auto track_node = track_map_node.child("Track"); track_node; track_node = track_node.next_sibling("Track"))			
			{
				auto source_track_name_node = track_node.child("SourceTrackName");
				if (!source_track_name_node)
				{
					_last_error = "Failed to find SourceStreams/SourceStream/TrackMap/SourceTrackName node";
					return false;
				}

				auto new_track_name_node = track_node.child("NewTrackName");
				if (!new_track_name_node)
				{
					_last_error = "Failed to find SourceStreams/SourceStream/TrackMap/NewTrackName node";
					return false;
				}
				
				ov::String new_track_name = new_track_name_node.text().as_string();
				source_stream->AddTrackMap(source_track_name_node.text().as_string(), new_track_name);
				_new_track_names.emplace(new_track_name, true);
			}

			_source_streams.push_back(source_stream);
		}

		return true;
	}

	ov::String MultiplexProfile::GetLastError() const
	{
		return _last_error;
	}

	CommonErrorCode MultiplexProfile::SaveToXMLFile(const ov::String &file_path) const
	{
		return CommonErrorCode::SUCCESS;
	}

	CommonErrorCode MultiplexProfile::ToJsonObject(Json::Value &root_object) const
	{
		return CommonErrorCode::SUCCESS;
	}

	std::chrono::system_clock::time_point MultiplexProfile::GetCreatedTime() const
	{
		return _created_time;
	}

	ov::String MultiplexProfile::GetOutputStreamName() const
	{
		return _output_stream_name;
	}

	const std::vector<std::shared_ptr<info::Playlist>> &MultiplexProfile::GetPlaylists() const
	{
		return _playlists;
	}

	const std::vector<std::shared_ptr<MultiplexProfile::SourceStream>> &MultiplexProfile::GetSourceStreams() const
	{
		return _source_streams;
	}
}