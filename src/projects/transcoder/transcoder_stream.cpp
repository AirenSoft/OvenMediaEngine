//==============================================================================
//
//  TranscoderStream
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_stream.h"

#include "config/config_manager.h"
#include "modules/transcode_webhook/transcode_webhook.h"
#include "orchestrator/orchestrator.h"

#include "transcoder_application.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 100
#define FILLER_ENABLED true
#define UNUSED_VARIABLE(var) (void)var;

// max initial media packet buffer size, for OOM protection
#define MAX_INITIAL_MEDIA_PACKET_BUFFER_SIZE 10000

std::shared_ptr<TranscoderStream> TranscoderStream::Create(const info::Application &application_info, const std::shared_ptr<info::Stream> &org_stream_info, TranscodeApplication *parent)
{
	auto stream = std::make_shared<TranscoderStream>(application_info, org_stream_info, parent);
	if (stream == nullptr)
	{
		return nullptr;
	}

	return stream;
}

TranscoderStream::TranscoderStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _parent(parent), _application_info(application_info), _input_stream(stream)
{
	_log_prefix = ov::String::FormatString("[%s/%s(%u)]", _application_info.GetVHostAppName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	// default output profiles configuration
	_output_profiles_cfg = &(_application_info.GetConfig().GetOutputProfiles());

	logtd("%s Trying to create transcode stream", _log_prefix.CStr());
}

TranscoderStream::~TranscoderStream()
{
	Stop();
}

info::stream_id_t TranscoderStream::GetStreamId()
{
	if (_input_stream != nullptr)
	{
		return _input_stream->GetId();
	}

	return 0;
}

bool TranscoderStream::Start()
{
	if (GetState() != State::CREATED)
	{
		return false;
	}

	SetState(State::PREPARING);

	logti("%s stream has been started", _log_prefix.CStr());

	return true;
}

bool TranscoderStream::Prepare(const std::shared_ptr<info::Stream> &stream)
{
	if(GetState() != State::PREPARING)
	{
		logte("%s stream has already prepared", _log_prefix.CStr());		
		return false;
	}

	// Transcoder Webhook
	_output_profiles_cfg = RequestWebhoook();
	if (_output_profiles_cfg == nullptr)
	{
		logtw("%s There is no output profiles", _log_prefix.CStr());
		return false;
	}

	// Create Ouput Streams & Notify to create a new stream on the media router.
	if (!StartInternal())
	{
		SetState(State::ERROR);

		return false;
	}

	// Create Decoders
	if(!PrepareInternal())
	{
		SetState(State::ERROR);

		return false;
	}

	SetState(State::STARTED);
	
	logti("%s stream has been prepared", _log_prefix.CStr());

	return true;
}

bool TranscoderStream::Update(const std::shared_ptr<info::Stream> &stream)
{
	if(GetState() != State::STARTED)
	{
		logtd("%s stream is not started", _log_prefix.CStr());
		return false;
	}

	return UpdateInternal(stream);
}


bool TranscoderStream::Stop()
{
	if(GetState() == State::STOPPED)
	{
		return true;
	}

	logtd("%s Wait for stream thread to terminated", _log_prefix.CStr());

	RemoveDecoders();
	RemoveFilters();
	RemoveEncoders();

	// Delete all composite of components
	_link_input_to_outputs.clear();
	
	_link_input_to_decoder.clear();
	_link_decoder_to_filters.clear();
	_link_filter_to_encoder.clear();
	_link_encoder_to_outputs.clear();

	// Delete all last decoded frame information
	_last_decoded_frame_pts.clear();
	_last_decoded_frames.clear();

	// Notify to delete the stream created on the MediaRouter
	NotifyDeleteStreams();

	// Delete all output streams information
	_output_streams.clear();

	SetState(State::STOPPED);

	logti("%s stream has been stopped", _log_prefix.CStr());

	return true;
}

const cfg::vhost::app::oprf::OutputProfiles* TranscoderStream::RequestWebhoook()
{
	TranscodeWebhook webhook(_application_info);
	
	auto policy = webhook.RequestOutputProfiles(*_input_stream, _remote_output_profiles);

	if (policy == TranscodeWebhook::Policy::DeleteStream)
	{
		logtw("%s Delete a stream by transcode webhook", _log_prefix.CStr());
		ocst::Orchestrator::GetInstance()->TerminateStream(_application_info.GetVHostAppName(), _input_stream->GetName());
		return nullptr;
	}		
	else if (policy == TranscodeWebhook::Policy::CreateStream)
	{
		logti("%s Using external output profiles by webhook", _log_prefix.CStr());		
		logti("%s OutputProfile\n%s", _log_prefix.CStr(), _remote_output_profiles.ToString().CStr());		
		return &_remote_output_profiles;
	}
	else if (policy == TranscodeWebhook::Policy::UseLocalProfiles)
	{
		logti("%s Using local output profiles by webhook", _log_prefix.CStr());		
		logtd("%s OutputProfile \n%s", _log_prefix.CStr(), _application_info.GetConfig().GetOutputProfiles().ToString().CStr());		

		return &(_application_info.GetConfig().GetOutputProfiles());;
	}

	return nullptr;
}

bool TranscoderStream::StartInternal()
{
	// If the application is created by Dynamic, make it bypass in Default Stream.
	if (_application_info.IsDynamicApp() == true)
	{
		if (CreateOutputStreamDynamic() == 0)
		{
			logte("No output stream generated");
			return false;
		}
	}
	else
	{
		if (CreateOutputStreams() == 0)
		{
			logte("No output stream generated");
			return false;
		}
	}

	// Notify to create a new stream on the media router.
	NotifyCreateStreams();

	return true;
}

bool TranscoderStream::PrepareInternal()
{
	if (BuildComposite() == 0)
	{
		logte("No components generated");
		return false;
	}

	if (CreateDecoders() == 0)
	{
		logti("No decoder generated");
	}

	StoreTracks(_input_stream);

	return true;
}

bool TranscoderStream::UpdateInternal(const std::shared_ptr<info::Stream> &stream)
{
	// Check if smooth stream transition is possible
	// [Rule]
	// - The number of tracks per media type should not exceed one.
	// - The input track should not change.
	_is_updating = true;

	if (CanSeamlessTransition(stream) == true)
	{
		logtd("%s This stream support seamless transitions", _log_prefix.CStr());
		FlushBuffers();

		RemoveDecoders();
		RemoveFilters();

		CreateDecoders();
	}
	else
	{
		logtw("%s This stream does not support seamless transitions. Renewing all", _log_prefix.CStr());

		FlushBuffers();

		RemoveDecoders();
		RemoveFilters();
		RemoveEncoders();

		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());
		NotifyUpdateStreams();

		StoreTracks(stream);
	}

	_is_updating = false;

	return true;
}

void TranscoderStream::FlushBuffers()
{
	for (auto &[id, object] : _encoders)
	{
		auto filter = object.first;
		if (filter != nullptr)
		{
			filter->Flush();
		}

		auto encoder = object.second;
		if (encoder != nullptr)
		{
			encoder->Flush();
		}
	}
}

void TranscoderStream::RemoveDecoders()
{
	std::unique_lock<std::shared_mutex> decoder_lock(_decoder_map_mutex);

	auto decoders = _decoders;
	_decoders.clear();

	decoder_lock.unlock();

	for (auto &[id, object] : decoders)
	{
		if (object != nullptr)
		{
			object->Stop();
			object.reset();
		}
	}
}

void TranscoderStream::RemoveFilters()
{
	std::unique_lock<std::shared_mutex> filter_lock(_filter_map_mutex);

	auto filters = _filters;
	_filters.clear();

	filter_lock.unlock();

	for (auto &[id, object] : filters)
	{
		if (object != nullptr)
		{
			object->Stop();
			object.reset();
		}
	}
}

void TranscoderStream::RemoveEncoders()
{
	std::unique_lock<std::shared_mutex> encoder_lock(_encoder_map_mutex);
	
	auto encoders = _encoders;
	_encoders.clear();
	
	encoder_lock.unlock();

	for (auto &[id, object] : encoders)
	{
		auto filter = object.first;
		if (filter != nullptr)
		{
			filter->Stop();
			filter.reset();
		}

		auto encoder = object.second;
		if (encoder != nullptr)
		{
			encoder->Stop();
			encoder.reset();
		}
	}
}

std::shared_ptr<MediaTrack> TranscoderStream::GetInputTrack(MediaTrackId track_id)
{
	if (_input_stream)
	{
		return _input_stream->GetTrack(track_id);
	}

	return nullptr;
}

std::shared_ptr<info::Stream> TranscoderStream::GetInputStream()
{
	return _input_stream;
}

std::shared_ptr<info::Stream> TranscoderStream::GetOutputStreamByTrackId(MediaTrackId output_track_id)
{
	for (auto &iter : _output_streams)
	{
		auto stream = iter.second;
		if (stream->GetTrack(output_track_id) != nullptr)
		{
			return stream;
		}
	}

	return nullptr;
}

bool TranscoderStream::CanSeamlessTransition(const std::shared_ptr<info::Stream> &input_stream)
{
	auto new_tracks = input_stream->GetTracks();

	// Check if the number and type of original tracks are different.
	if(CompareTracksForSeamlessTransition(new_tracks, GetStoredTracks()) == false)
	{
		logtw("%s The input track has changed. It does not support smooth transitions.", _log_prefix.CStr());
		return false;
	}
	
	return true;
}

bool TranscoderStream::Push(std::shared_ptr<MediaPacket> packet)
{
	if (GetState() == State::STARTED)
	{
		if (_initial_media_packet_buffer.IsEmpty() == false)
		{
			SendBufferedPackets();
		}

		ProcessPacket(std::move(packet));
	}
	else if (GetState() == State::CREATED || GetState() == State::PREPARING)
	{
		BufferMediaPacketUntilReadyToPlay(packet);
	}
	else if (GetState() == State::ERROR)
	{
		return false;
	}

	// State::STOPPED : Do nothing

	return true;
}

void TranscoderStream::BufferMediaPacketUntilReadyToPlay(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_initial_media_packet_buffer.Size() >= MAX_INITIAL_MEDIA_PACKET_BUFFER_SIZE)
	{
		// Drop the oldest packet, for OOM protection
		_initial_media_packet_buffer.Dequeue(0);
	}

	_initial_media_packet_buffer.Enqueue(media_packet);
}

bool TranscoderStream::SendBufferedPackets()
{
	logtd("SendBufferedPackets - BufferSize (%u)", _initial_media_packet_buffer.Size());
	while (_initial_media_packet_buffer.IsEmpty() == false)
	{
		auto buffered_media_packet = _initial_media_packet_buffer.Dequeue();
		if (buffered_media_packet.has_value() == false)
		{
			continue;
		}

		auto media_packet = buffered_media_packet.value();
	
		ProcessPacket(std::move(media_packet));
	}

	return true;
}

// Dynamically generated applications are created by default with BYPASS profiles.
int32_t TranscoderStream::CreateOutputStreamDynamic()
{
	// Number of generated output streams
	int32_t created_count = 0;

	auto output_stream = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);
	if (output_stream == nullptr)
	{
		return created_count;
	}

	output_stream->SetName(_input_stream->GetName());
	output_stream->SetMediaSource(_input_stream->GetUUID());
	output_stream->LinkInputStream(_input_stream);

	for (auto &[input_track_id, input_track] : _input_stream->GetTracks())
	{
		UNUSED_VARIABLE(input_track_id)

		auto output_track = input_track->Clone();
		if (output_track == nullptr)
		{
			continue;
		}

		output_track->SetBypass(true);
		output_track->SetId(NewTrackId());

		output_stream->AddTrack(output_track);

		AddComposite("dynamic", _input_stream, input_track, output_stream, output_track);
	}

	// Add to Output Stream List. The key is the output stream name.
	_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream)); 

	logti("%s Output stream(dynamic) has been created. [%s/%s(%u)]",
		  _log_prefix.CStr(),
		  _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

	created_count++;

	return created_count;
}

int32_t TranscoderStream::CreateOutputStreams()
{
	int32_t created_count = 0;

	// Get the output profile to make the output stream
	auto cfg_output_profile_list = GetOutputProfilesCfg()->GetOutputProfileList();
	
	for (const auto &profile : cfg_output_profile_list)
	{
		auto output_stream = CreateOutputStream(profile);
		if (output_stream == nullptr)
		{
			logte("Could not create output stream");
			continue;
		}

		_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream));

		logti("%s Output stream has been created. [%s/%s(%u)]", _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

		created_count++;
	}

	return created_count;
}

std::shared_ptr<info::Stream> TranscoderStream::CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile)
{
	if (cfg_output_profile.GetOutputStreamName().IsEmpty())
	{
		return nullptr;
	}

	auto output_stream = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);
	if (output_stream == nullptr)
	{
		return nullptr;
	}

	// It helps modules to recognize origin stream from provider
	output_stream->LinkInputStream(_input_stream);
	output_stream->SetMediaSource(_input_stream->GetUUID());

	// Create a output stream name.
	auto name = cfg_output_profile.GetOutputStreamName();
	if (::strstr(name.CStr(), "${OriginStreamName}") != nullptr)
	{
		name = name.Replace("${OriginStreamName}", _input_stream->GetName());
	}
	output_stream->SetName(name);

	// Create a Output Track
	for (auto &[input_track_id, input_track] : _input_stream->GetTracks())
	{
		switch (input_track->GetMediaType())
		{
			case cmn::MediaType::Video: {
				// Video Profile
				for (auto &profile : cfg_output_profile.GetEncodes().GetVideoProfileList())
				{
					auto output_track = CreateOutputTrack(input_track, profile);
					if (output_track == nullptr)
					{
						logtw("Failed to create media tracks. Encoding options need to be checked. InputTrack(%d)", input_track_id);
						continue;
					}

					output_stream->AddTrack(output_track);

					AddComposite(ProfileToSerialize(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
				}

				// Image Profile
				for (auto &profile : cfg_output_profile.GetEncodes().GetImageProfileList())
				{
					auto output_track = CreateOutputTrack(input_track, profile);
					if (output_track == nullptr)
					{
						logtw("Failed to create media tracks. Encoding options need to be checked. InputTrack(%d)", input_track_id);
						continue;
					}

					output_stream->AddTrack(output_track);

					AddComposite(ProfileToSerialize(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
				}
			}
			break;
			case cmn::MediaType::Audio: {
				// Audio Profile
				for (auto &profile : cfg_output_profile.GetEncodes().GetAudioProfileList())
				{
					auto output_track = CreateOutputTrack(input_track, profile);
					if (output_track == nullptr)
					{
						logtw("Failed to create media tracks. Encoding options need to be checked. InputTrack(%d)", input_track_id);
						continue;
					}

					output_stream->AddTrack(output_track);

					AddComposite(ProfileToSerialize(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
				}
			}
			break;
			
			// If there is a data type track in the input stream, it must be created equally in all output streams.
			case cmn::MediaType::Data: {
					auto output_track = CreateOutputTrackDataType(input_track);
					if (output_track == nullptr)
					{
						logtw("Failed to create media tracks. Encoding options need to be checked. InputTrack(%d)", input_track_id);
						continue;
					}

					output_stream->AddTrack(output_track);

					AddComposite(ProfileToSerialize(input_track_id), _input_stream, input_track, output_stream, output_track);
			}
			break;
			default: {
				logtw("Unsupported media type of input track. type(%d)", input_track->GetMediaType());
				continue;
			}
		}
	}

	// Playlist
	bool is_parsed = false;
	auto cfg_playlists = cfg_output_profile.GetPlaylists(&is_parsed);
	if (is_parsed)
	{
		for (const auto &cfg_playlist : cfg_playlists)
		{
			auto playlist_info = cfg_playlist.GetPlaylistInfo();

			// Create renditions with RenditionTemplate
			auto rendition_templates = cfg_playlist.GetRenditionTemplates();
			auto tracks = output_stream->GetTracks();

			for (const auto &rendition_template : rendition_templates)
			{
				bool has_video_template = false, has_audio_template = false;
				auto video_template = rendition_template.GetVideoTemplate(&has_video_template);
				auto audio_template = rendition_template.GetAudioTemplate(&has_audio_template);

				std::vector<std::shared_ptr<MediaTrack>> matched_video_tracks, matched_audio_tracks;

				if (has_video_template)
				{
					for (const auto &[track_id, track] : tracks)
					{
						if (video_template.IsMatched(track) == false)
						{
							continue;
						}

						// Separate the Track from the existing Variant group.
						matched_video_tracks.push_back(track);
					}
				}
				
				if (has_audio_template)
				{
					// Do not separate the Audio track group. (Used as Multilingual)
					for (const auto &[group_name, group] : output_stream->GetMediaTrackGroups())
					{
						auto track = group->GetFirstTrack();
						if (audio_template.IsMatched(track) == false)
						{
							continue;
						}

						matched_audio_tracks.push_back(track);
					}
				}

				if (matched_video_tracks.empty() && matched_audio_tracks.empty())
				{
					logtw("No matched tracks for the rendition template (%s).", rendition_template.GetName().CStr());
					continue;
				}

				if (matched_video_tracks.empty() == false && matched_audio_tracks.empty() == false)
				{
					for (const auto &video_track : matched_video_tracks)
					{
						for (const auto &audio_track : matched_audio_tracks)
						{
							// Make Rendition Name
							ov::String rendition_name = MakeRenditionName(rendition_template.GetName(), playlist_info, video_track, audio_track);
							
							auto rendition = std::make_shared<info::Rendition>(rendition_name, video_track->GetVariantName(), audio_track->GetVariantName());
							rendition->SetVideoIndexHint(video_track->GetGroupIndex());
							rendition->SetAudioIndexHint(audio_track->GetGroupIndex());

							playlist_info->AddRendition(rendition);

							logti("Rendition(%s) has been created from template in Playlist(%s) : video(%s/%d), audio(%s%d)", rendition_name.CStr(), playlist_info->GetName().CStr(), video_track->GetPublicName().CStr(), video_track->GetGroupIndex(), audio_track->GetPublicName().CStr(), audio_track->GetGroupIndex());
						}
					}
				}
				else if (matched_video_tracks.empty() == false)
				{
					for (const auto &video_track : matched_video_tracks)
					{
						// Make Rendition Name
						ov::String rendition_name = MakeRenditionName(rendition_template.GetName(), playlist_info, video_track, nullptr);
						auto rendition = std::make_shared<info::Rendition>(rendition_name, video_track->GetVariantName(), "");
						rendition->SetVideoIndexHint(video_track->GetGroupIndex());

						playlist_info->AddRendition(rendition);

						logti("Rendition(%s) has been created from template in Playlist(%s) : video(%s/%d)", rendition_name.CStr(), playlist_info->GetName().CStr(), video_track->GetPublicName().CStr(), video_track->GetGroupIndex());
					}
				}
				else if (matched_audio_tracks.empty() == false)
				{
					for (const auto &audio_track : matched_audio_tracks)
					{
						// Make Rendition Name
						ov::String rendition_name = MakeRenditionName(rendition_template.GetName(), playlist_info, nullptr, audio_track);

						auto rendition = std::make_shared<info::Rendition>(rendition_name, "", audio_track->GetVariantName());
						rendition->SetAudioIndexHint(audio_track->GetGroupIndex());

						playlist_info->AddRendition(rendition);

						logti("Rendition(%s) has been created from template in Playlist(%s) : audio(%s/%d)", rendition_name.CStr(), playlist_info->GetName().CStr(), audio_track->GetPublicName().CStr(), audio_track->GetGroupIndex());
					}
				}
			}

			logti("Playlist(%s) has been created", playlist_info->GetName().CStr());
			logti("%s", playlist_info->ToString().CStr());

			output_stream->AddPlaylist(playlist_info);
		}
	}

	return output_stream;
}

ov::String TranscoderStream::MakeRenditionName(const ov::String &name_template, const std::shared_ptr<info::Playlist> &playlist_info, const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track)
{
	ov::String rendition_name = name_template;

	if (video_track != nullptr)
	{
		rendition_name = rendition_name.Replace("${Width}", ov::String::FormatString("%d", video_track->GetWidth()).CStr());
		rendition_name = rendition_name.Replace("${Height}", ov::String::FormatString("%d", video_track->GetHeight()).CStr());
		rendition_name = rendition_name.Replace("${Bitrate}", ov::String::FormatString("%d", video_track->GetBitrate()).CStr());
		rendition_name = rendition_name.Replace("${Framerate}", ov::String::FormatString("%d", video_track->GetFrameRate()).CStr());
	}

	if (audio_track != nullptr)
	{
		rendition_name = rendition_name.Replace("${Samplerate}", ov::String::FormatString("%d", audio_track->GetSampleRate()).CStr());
		rendition_name = rendition_name.Replace("${Channel}", ov::String::FormatString("%u", audio_track->GetChannel().GetCounts()).CStr());
	}

	// Check if the rendition name is duplicated
	uint32_t rendition_index = 0;
	ov::String unique_rendition_name = rendition_name;
	while (playlist_info->GetRendition(unique_rendition_name) != nullptr)
	{
		unique_rendition_name = ov::String::FormatString("%s_%d", rendition_name.CStr(), ++rendition_index);
	}

	return unique_rendition_name;
}

int32_t TranscoderStream::BuildComposite()
{
	int32_t created_count = 0;

	for (auto &[key, composite] : _composite_map)
	{
		UNUSED_VARIABLE(key)
		auto input_track 	= composite->GetInputTrack();

		auto input_track_id = input_track->GetId();
		auto decoder_id 	= input_track->GetId();

		auto filter_id 		= composite->GetId();
		auto encoder_id 	= composite->GetId();

		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			auto output_stream_and_track = std::make_pair(output_stream, output_track->GetId());

			// Bypass Flow: InputTrack -> OutputTrack
			if (output_track->IsBypass() == true)
			{
				_link_input_to_outputs[input_track_id].push_back(output_stream_and_track);
			}
			// Transcoding Flow: InputTrack -> Decoder -> Filter -> Encoder -> OutputTrack
			else
			{
				// Decoding: InputTrack(1) -> Decoder(1)
				_link_input_to_decoder[input_track_id] = decoder_id;

				// Rescale/Resample Filtering: Decoder(1) -> Filter (N)
				auto &filter_ids = _link_decoder_to_filters[decoder_id];
				if (std::find(filter_ids.begin(), filter_ids.end(), filter_id) == filter_ids.end())
				{
					filter_ids.push_back(filter_id);
				}

				// Encoding: Filter(1) -> Encoder (1)
				_link_filter_to_encoder[filter_id] = encoder_id;

				// Flushing: Encoder(1) -> OutputTrack (N)
				_link_encoder_to_outputs[encoder_id].push_back(output_stream_and_track);
			}
		}

		created_count++;
	}

	logtd("%s", GetInfoStringComposite().CStr());

	return created_count;
}

// LOG for DEBUG
ov::String TranscoderStream::GetInfoStringComposite()
{
	ov::String debug_log = "Component composition list\n";

	for (auto &[input_track_id, input_track] : _input_stream->GetTracks())
	{
		bool matched = false;
		debug_log.AppendFormat("* Input(%s/%u) : %s\n",
							   _input_stream->GetName().CStr(),
							   input_track_id,
							   input_track->GetInfoString().CStr());

		// Bypass Stream
		if (_link_input_to_outputs.find(input_track_id) != _link_input_to_outputs.end())
		{
			matched = true;
			auto &output_tracks = _link_input_to_outputs[input_track_id];
			for (auto &[stream, track_id] : output_tracks)
			{
				auto output_track = stream->GetTrack(track_id);

				debug_log.AppendFormat("  + Output(%s/%d) : Passthrough, %s\n",
									   stream->GetName().CStr(),
									   track_id,
									   output_track->GetInfoString().CStr());
			}
		}

		// Transcoding Stream
		if (_link_input_to_decoder.find(input_track_id) != _link_input_to_decoder.end())
		{
			matched = true;

			auto decoder_id = _link_input_to_decoder[input_track_id];
			debug_log.AppendFormat("  + Decoder(%u)\n", decoder_id);

			if (_link_decoder_to_filters.find(decoder_id) != _link_decoder_to_filters.end())
			{
				auto &filter_ids = _link_decoder_to_filters[decoder_id];
				for (auto &filter_id : filter_ids)
				{
					debug_log.AppendFormat("    + Filter(%u)\n", filter_id);

					if (_link_filter_to_encoder.find(filter_id) != _link_filter_to_encoder.end())
					{
						auto encoder_id = _link_filter_to_encoder[filter_id];
						debug_log.AppendFormat("      + Encoder(%d)\n", encoder_id);

						if (_link_encoder_to_outputs.find(encoder_id) != _link_encoder_to_outputs.end())
						{
							auto &output_tracks = _link_encoder_to_outputs[encoder_id];
							for (auto &[stream, track_id] : output_tracks)
							{
								auto output_track = stream->GetTrack(track_id);

								debug_log.AppendFormat("        + Output(%s/%u) : %s\n",
													   stream->GetName().CStr(),
													   track_id,
													   output_track->GetInfoString().CStr());
							}
						}
					}
				}
			}
		}

		if(matched == false)
		{
			debug_log.AppendFormat("  + (No output)\n");
		}
	}

	return debug_log;
}

void TranscoderStream::AddComposite(
	ov::String serialized_profile,
	std::shared_ptr<info::Stream> input_stream,	std::shared_ptr<MediaTrack> input_track,
	std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	auto key = std::make_pair(serialized_profile, input_track->GetMediaType());

	if (_composite_map.find(key) == _composite_map.end())
	{
		auto composite = std::make_shared<CompositeContext>(_last_composite_id++);
		composite->SetInput(input_stream, input_track);

		_composite_map[key] = composite;
	}

	_composite_map[key]->AddOutput(output_stream, output_track);
}

int32_t TranscoderStream::CreateDecoders()
{
	int32_t created_count = 0;

	for (auto &[input_track_id, decoder_id] : _link_input_to_decoder)
	{
		// Get Input Track
		auto it = GetInputStream()->GetTracks().find(input_track_id);
		if (it == GetInputStream()->GetTracks().end())
		{
			continue;
		}
		auto &input_track = it->second;

		// Create Decoder
		if (CreateDecoder(decoder_id, GetInputStream(), input_track) == false)
		{
			continue;
		}

		created_count++;
	}

	return created_count;
}

bool TranscoderStream::CreateDecoder(MediaTrackId decoder_id, std::shared_ptr<info::Stream> input_stream, std::shared_ptr<MediaTrack> input_track)
{
	if(GetDecoder(decoder_id) != nullptr)
	{
		logtw("%s Decoder already exists. InputTrack(%d) > Decoder(%d)", _log_prefix.CStr(), input_track->GetId(), decoder_id);
		return true;
	}

	// Set the keyframe decode only flag for the decoder.
	if (input_track->GetMediaType() == cmn::MediaType::Video &&
		GetOutputProfilesCfg()->GetDecodes().IsOnlyKeyframes() == true)
	{
		input_track->SetKeyframeDecodeOnly(IsKeyframeOnlyDecodable(_output_streams));
	}

	// Set the thread count for the decoder.
	input_track->SetThreadCount(GetOutputProfilesCfg()->GetDecodes().GetThreadCount());

	auto hwaccels_enable = GetOutputProfilesCfg()->GetHWAccels().GetDecoder().IsEnable() ||
						   GetOutputProfilesCfg()->IsHardwareAcceleration();  // Deprecated

	auto hwaccels_modules = GetOutputProfilesCfg()->GetHWAccels().GetDecoder().GetModules();

	// Get a list of available decoder candidates.
	auto candidates = TranscodeDecoder::GetCandidates(hwaccels_enable, hwaccels_modules, input_track);
	if (candidates == nullptr)
	{
		logte("%s Decoder candidates are not found. InputTrack(%u)", _log_prefix.CStr(), input_track->GetId());
		return false;
	}

	// Create a decoder
	auto decoder = TranscodeDecoder::Create(
		decoder_id,
		input_stream,
		input_track,
		candidates,
		bind(&TranscoderStream::OnDecodedFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	if (decoder == nullptr)
	{
		logte("%s Decoder allocation failed.  InputTrack(%u) > Decoder(%u)", _log_prefix.CStr(), input_track->GetId(), decoder_id);
		return false;
	}

	SetDecoder(decoder_id, decoder);

	logtd("%s Created decoder. InputTrack(%u) > Decoder(%u)", _log_prefix.CStr(), input_track->GetId(), decoder_id);

	return true;
}

std::shared_ptr<TranscodeDecoder> TranscoderStream::GetDecoder(MediaTrackId decoder_id)
{
	std::shared_lock<std::shared_mutex> decoder_lock(_decoder_map_mutex);
	if (_decoders.find(decoder_id) == _decoders.end())
	{
		return nullptr;
	}

	return _decoders[decoder_id];
}

void TranscoderStream::SetDecoder(MediaTrackId decoder_id, std::shared_ptr<TranscodeDecoder> decoder)
{
	std::unique_lock<std::shared_mutex> decoder_lock(_decoder_map_mutex);

	_decoders[decoder_id] = decoder;
}

int32_t TranscoderStream::CreateEncoders(std::shared_ptr<MediaFrame> buffer)
{
	int32_t created = 0;
	MediaTrackId track_id = buffer->GetTrackId();

	for (auto &[key, composite] : _composite_map)
	{
		UNUSED_VARIABLE(key)

		if ((uint32_t)track_id != composite->GetInputTrack()->GetId())
		{
			continue;
		}

		auto encoder_id = composite->GetId();
		auto output_tracks_it = _link_encoder_to_outputs.find(encoder_id);
		if (output_tracks_it == _link_encoder_to_outputs.end())
		{
			continue;
		}

		auto output_tracks = output_tracks_it->second;
		if (output_tracks.size() == 0)
		{
			continue;
		}

		for (auto &[output_stream, output_track_id] : output_tracks)
		{
			auto output_track = output_stream->GetTrack(output_track_id);

			if (CreateEncoder(encoder_id, output_stream, output_track) == false)
			{
				logte("[%s/%s(%u)] Could not create encoder. Encoder(%d), OutputTrack(%d)", _application_info.GetVHostAppName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), encoder_id, output_track->GetId());
				continue;
			}

			created++;
		}
	}

	return created;
}

bool TranscoderStream::CreateEncoder(MediaTrackId encoder_id, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	if (GetEncoder(encoder_id).has_value())
	{
		logtd("%s Encoder already exists. Encoder(%d)", _log_prefix.CStr(), encoder_id);
		return true;
	}

	auto hwaccels_enable = GetOutputProfilesCfg()->GetHWAccels().GetEncoder().IsEnable() ||
						   GetOutputProfilesCfg()->IsHardwareAcceleration();  // Deprecated

	auto hwaccels_modules = GetOutputProfilesCfg()->GetHWAccels().GetEncoder().GetModules();

	// Get a list of available encoder candidates.
	auto candidates = TranscodeEncoder::GetCandidates(hwaccels_enable, hwaccels_modules, output_track);
	if (candidates == nullptr)
	{
		logte("%s Decoder candidates are not found. InputTrack(%d)", _log_prefix.CStr(), output_track->GetId());
		return false;
	}

	// Create Encoder
	auto encoder = TranscodeEncoder::Create(encoder_id, output_stream, output_track, candidates,
											bind(&TranscoderStream::OnEncodedPacket, this, std::placeholders::_1, std::placeholders::_2));
	if (encoder == nullptr)
	{
		return false;
	}

	// Set the sample format and color space supported by the encoder to the output track.
	// These values are used in the Resampler/Rescaler filter.
	if (output_track->GetMediaType() == cmn::MediaType::Video)
	{
		output_track->SetColorspace(encoder->GetSupportedFormat());
	}
	else if (output_track->GetMediaType() == cmn::MediaType::Audio)
	{
		output_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(encoder->GetSupportedFormat()));
	}

	// Create Paired Filter
	std::shared_ptr<TranscodeFilter> filter = nullptr;
	if(output_track->GetMediaType() == cmn::MediaType::Audio)
	{
		filter = TranscodeFilter::Create(encoder_id, output_stream, output_track,
										 bind(&TranscoderStream::OnPreEncodeFilteredFrame, this, std::placeholders::_1, std::placeholders::_2));
		if (filter == nullptr)
		{
			// Stop & Release Encoder
			encoder->Stop();
			encoder.reset();

			return false;
		}
	}

	SetEncoder(encoder_id, filter, encoder);

	logtd("%s Created encoder. Encoder(%d) > OutputTrack(%d)", _log_prefix.CStr(), encoder_id, output_track->GetId());

	return true;
}

std::optional<std::pair<std::shared_ptr<TranscodeFilter>, std::shared_ptr<TranscodeEncoder>>> TranscoderStream::GetEncoder(MediaTrackId encoder_id)
{
	std::shared_lock<std::shared_mutex> encoder_lock(_encoder_map_mutex);
	if (_encoders.find(encoder_id) == _encoders.end())
	{
		return std::nullopt;
	}

	return _encoders[encoder_id];
}

void TranscoderStream::SetEncoder(MediaTrackId encoder_id, std::shared_ptr<TranscodeFilter> filter, std::shared_ptr<TranscodeEncoder> encoder)
{
	std::unique_lock<std::shared_mutex> encoder_lock(_encoder_map_mutex);

	_encoders[encoder_id] = std::make_pair(filter, encoder);
}

int32_t TranscoderStream::CreateFilters(std::shared_ptr<MediaFrame> buffer)
{
	int32_t created = 0;

	MediaTrackId decoder_id = buffer->GetTrackId();

	// 1. Get Decoder -> Filter List
	auto decoder_to_filters_it = _link_decoder_to_filters.find(decoder_id);
	if (decoder_to_filters_it == _link_decoder_to_filters.end())
	{ 
		logtw("%s Could not found filter list related to decoder", _log_prefix.CStr());

		return created;
	}

	// 2. Get Output Track of Encoders
	auto filter_ids = decoder_to_filters_it->second;
	for (auto &filter_id : filter_ids)
	{
		MediaTrackId encoder_id = _link_filter_to_encoder[filter_id];
		auto encoder_set = GetEncoder(encoder_id);
		if(encoder_set.has_value() == false)
		{
			logte("%s Failed to create filter. could not found encoder set. Encoder(%d), Filter(%d)", _log_prefix.CStr(), encoder_id, filter_id);
			continue;
		}

		auto encoder = encoder_set->second;
		if(encoder == nullptr)
		{
			logte("%s Failed to create filter. could not found encoder. Encoder(%d), Filter(%d)", _log_prefix.CStr(), encoder_id, filter_id);
			continue;
		}

		auto decoder = GetDecoder(decoder_id);
		if(decoder == nullptr)
		{
			logte("%s Failed to create filter. could not found decoder. Decoder(%d), Filter(%d)", _log_prefix.CStr(), decoder_id, filter_id);
			continue;
		}

		auto input_track = decoder->GetRefTrack();
		auto output_track = encoder->GetRefTrack();
		if(input_track == nullptr || output_track == nullptr)
		{
			logte("%s Failed to create filter. could not found input or output track. Decoder(%d), Encoder(%d), Filter(%d)", _log_prefix.CStr(), decoder_id, encoder_id, filter_id);
			continue;
		}

		if (CreateFilter(filter_id, input_track, output_track) == false)
		{
			continue;
		}

		created++;
	}

	return created;
}

bool TranscoderStream::CreateFilter(MediaTrackId filter_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<MediaTrack> output_track)
{
	if(GetFilter(filter_id) != nullptr)
	{
		logtw("%s Filter already exists. Filter(%d)", _log_prefix.CStr(), filter_id);
		return true;
	}

	auto input_stream = GetInputStream();
	if(input_stream == nullptr)
	{
		logte("%s Could not found input stream", _log_prefix.CStr());
		return false;
	}

	auto output_stream = GetOutputStreamByTrackId(output_track->GetId());
	if(output_stream == nullptr)
	{
		logte("%s Could not found output stream", _log_prefix.CStr());
		return false;
	}

	auto filter = TranscodeFilter::Create(filter_id, input_stream, input_track, output_stream, output_track, bind(&TranscoderStream::OnFilteredFrame, this, std::placeholders::_1, std::placeholders::_2));
	if (filter == nullptr)
	{
		logte("%s Failed to create filter. Filter(%d)", _log_prefix.CStr(), filter_id);
		return false;
	}

	SetFilter(filter_id, filter);

	logtd("%s Created Filter. Filter(%d)", _log_prefix.CStr(), filter_id);

	return true;
}

std::shared_ptr<TranscodeFilter> TranscoderStream::GetFilter(MediaTrackId filter_id)
{
	std::shared_lock<std::shared_mutex> lock(_filter_map_mutex);
	if (_filters.find(filter_id) == _filters.end())
	{
		return nullptr;
	}

	return _filters[filter_id];
}

void TranscoderStream::SetFilter(MediaTrackId filter_id, std::shared_ptr<TranscodeFilter> filter)
{
	std::unique_lock<std::shared_mutex> lock(_filter_map_mutex);

	_filters[filter_id] = filter;
}

// Function called when codec information is extracted or changed from the decoder
void TranscoderStream::ChangeOutputFormat(std::shared_ptr<MediaFrame> buffer)
{
	logtd("%s Changed output format. InputTrack(%u)", _log_prefix.CStr(), buffer->GetTrackId());

	std::unique_lock<std::shared_mutex> lock(_format_change_mutex);

	if (buffer == nullptr)
	{
		logte("%s Invalid media buffer", _log_prefix.CStr());
		return;
	}

	// Update Track of Input Stream
	UpdateInputTrack(buffer);

	// Update Track of Output Stream
	UpdateOutputTrack(buffer);

	// Create an encoder. If there is an existing encoder, reuse it
	if(CreateEncoders(buffer) == 0)
	{
		logtw("%s No encoders have been created. InputTrack(%u)", _log_prefix.CStr(), buffer->GetTrackId());

		// SetState(State::ERROR);
		return;
	}

	// Create an filter. If there is an existing filter, reuse it
	if(CreateFilters(buffer) == 0)
	{
		logtw("%s No filters have been created. InputTrack(%u)", _log_prefix.CStr(), buffer->GetTrackId());
		
		// SetState(State::ERROR);
		return;
	}

}

// Information of the input track is updated by the decoded frame
void TranscoderStream::UpdateInputTrack(std::shared_ptr<MediaFrame> buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	logtd("%s Updated input track. InputTrack(%u)", _log_prefix.CStr(), track_id);

	auto &input_track = _input_stream->GetTrack(track_id);
	if (input_track == nullptr)
	{
		logte("Could not found output track. InputTrack(%u)", track_id);
		return;
	}

	switch (input_track->GetMediaType())
	{
		case cmn::MediaType::Video: 
		{
			input_track->SetWidth(buffer->GetWidth());
			input_track->SetHeight(buffer->GetHeight());
			input_track->SetColorspace(buffer->GetFormat());  // used AVPixelFormat
		}
		break;
		case cmn::MediaType::Audio: 
		{
			input_track->SetSampleRate(buffer->GetSampleRate());
			input_track->SetChannel(buffer->GetChannels());
			input_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(buffer->GetFormat()));
		}
		break;
		default: 
		{
			logtd("%s Unsupported media type. InputTrack(%d)", _log_prefix.CStr(), track_id);
		}
		break;
	}
}

// Update Output Track
void TranscoderStream::UpdateOutputTrack(std::shared_ptr<MediaFrame> buffer)
{
	MediaTrackId input_track_id = buffer->GetTrackId();

	for (auto &[key, composite] : _composite_map)
	{
		UNUSED_VARIABLE(key);

		// Get Related Composite
		if (input_track_id != composite->GetInputTrack()->GetId())
		{
			continue;
		}

		auto &input_track = _input_stream->GetTrack(input_track_id);


		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			UNUSED_VARIABLE(output_stream)

			logtd("%s Updated output track. OutputTrack(%u)", _log_prefix.CStr(), input_track_id, output_track->GetId());

			// Case of Passthrough
			if (output_track->IsBypass() == true)
			{
				UpdateOutputTrackPassthrough(output_track, buffer);
			}
			// Case Of Transcode
			else
			{
				UpdateOutputTrackTranscode(output_track, input_track, buffer);
			}
		}
	}
}

void TranscoderStream::ProcessPacket(const std::shared_ptr<MediaPacket> &packet)
{
	if (_input_stream->GetMsid() != packet->GetMsid() || packet == nullptr)
	{
		return;
	}

	BypassPacket(packet);

	DecodePacket(packet);
}

void TranscoderStream::BypassPacket(const std::shared_ptr<MediaPacket> &packet)
{
	MediaTrackId input_track_id = packet->GetTrackId();

	auto it = _link_input_to_outputs.find(input_track_id);
	if (it == _link_input_to_outputs.end())
	{
		return;
	}

	auto input_track = _input_stream->GetTrack(input_track_id);
	if (input_track == nullptr)
	{
		logte("%s Could not found input track. InputTrack(%d)", _log_prefix.CStr(), input_track_id);
		return;
	}

	for (auto &[output_stream, output_track_id] : it->second)
	{
		auto output_track = output_stream->GetTrack(output_track_id);
		if (output_track == nullptr)
		{
			logte("%s Could not found output track. OutputTrack(%d)", _log_prefix.CStr(), output_track_id);
			continue;
		}

		// Clone the packet and send it to the output stream.
		std::shared_ptr<MediaPacket> clone = nullptr;
		
		if (packet->GetBitstreamFormat() == cmn::BitstreamFormat::OVEN_EVENT)
		{
			auto event_packet = std::static_pointer_cast<MediaEvent>(packet);
			clone = event_packet->Clone();
		}
		else 
		{
			clone = packet->ClonePacket();
		}

		double scale = input_track->GetTimeBase().GetExpr() / output_track->GetTimeBase().GetExpr();
		clone->SetPts(static_cast<int64_t>((double)clone->GetPts() * scale));
		clone->SetDts(static_cast<int64_t>((double)clone->GetDts() * scale));
		clone->SetTrackId(output_track_id);

		SendFrame(output_stream, std::move(clone));
	}
}

void TranscoderStream::DecodePacket(const std::shared_ptr<MediaPacket> &packet)
{
	MediaTrackId input_track_id = packet->GetTrackId();

	auto it = _link_input_to_decoder.find(input_track_id);
	if (it == _link_input_to_decoder.end())
	{
		return;
	}

	auto decoder_id = it->second;
	auto decoder = GetDecoder(decoder_id);
	if (decoder == nullptr)
	{
		logte("%s Could not found decoder. Decoder(%d)", _log_prefix.CStr(), decoder_id);
		return;
	}
	decoder->SendBuffer(std::move(packet));
}

void TranscoderStream::OnDecodedFrame(TranscodeResult result, MediaTrackId decoder_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	if (decoded_frame == nullptr)
	{
		return;
	}

	if(_is_updating == true)
	{
		logtd("%s Current state is updating format. suspend decoded frame. Decoder(%d)", _log_prefix.CStr(), decoder_id);
		return;
	}

	switch (result)
	{
		case TranscodeResult::NoData: {
 #if FILLER_ENABLED
			///////////////////////////////////////////////////////////////////
			// Generate a filler frame (Part 1). * Using previously decoded frame
			///////////////////////////////////////////////////////////////////
			// - It is mainly used in Persistent Stream.
			// - When the input stream is switched, decoding fails until a KeyFrame is received. 
			//   If the keyframe interval is longer than the buffered length of the player, buffering occurs in the player.
			//   Therefore, the number of frames in which decoding fails is replaced with the last decoded frame and used as a filler frame.
			auto last_frame = GetLastDecodedFrame(decoder_id);
			if (last_frame == nullptr)
			{
				break;
			}

			auto input_track = GetInputTrack(decoder_id);
			auto input_track_of_filter = GetInputTrackOfFilter(decoder_id);
			if (input_track == nullptr || input_track_of_filter == nullptr)
			{
				break;
			}

			double input_expr = input_track->GetTimeBase().GetExpr();
			double filter_expr = input_track_of_filter->GetTimeBase().GetExpr();

			if(last_frame->GetPts()*filter_expr >= decoded_frame->GetPts()*input_expr)
			{
				break;
			}

			// The decoded frame pts should be modified to fit the Timebase of the filter input.
			last_frame->SetPts((int64_t)((double)decoded_frame->GetPts() * input_expr / filter_expr));

			// Record the timestamp of the last decoded frame. managed by microseconds.
			_last_decoded_frame_pts[decoder_id] = last_frame->GetPts() * filter_expr * 1000000.0;

			// logtd("%s Create filler frame because there is no decoding frame. Type(%s), Decoder(%u), FillerFrames(%d)"
			// 	, _log_prefix.CStr(), cmn::GetMediaTypeString(input_track->GetMediaType()).CStr(), decoder_id, 1);

			// Send Temporary Frame to Filter
			SpreadToFilters(decoder_id, last_frame);
#endif // End of Filler Frame Generation
		}
		break;

		// It indicates output format is changed
		case TranscodeResult::FormatChanged: {

			// Re-create filter and encoder using the format of decoded frame
			ChangeOutputFormat(decoded_frame);

 #if FILLER_ENABLED
			///////////////////////////////////////////////////////////////////
			// Generate a filler frame (Part 2). * Using latest decoded frame
			///////////////////////////////////////////////////////////////////
			// - It is mainly used in Schedule stream.
			// - When the input stream is changed, an empty section occurs in sequential frames. There is a problem with the A/V sync in the player.
			//   If there is a section where the frame is empty, may be out of sync in the player.
			//   Therefore, the filler frame is inserted in the hole of the sequential frame.
			auto input_track = GetInputTrack(decoder_id);
			if (!input_track)
			{
				logte("Could not found input track. Decoder(%d)", decoder_id);
				return;
			}

			if (_last_decoded_frame_pts.find(decoder_id) != _last_decoded_frame_pts.end())
			{
				auto last_decoded_frame_time_us = _last_decoded_frame_pts[decoder_id];
				auto last_decoded_frame_duration_us = _last_decoded_frame_duration[decoder_id];

				// Decoded frame PTS to microseconds
				int64_t curr_decoded_frame_time_us = (int64_t)((double)decoded_frame->GetPts() * input_track->GetTimeBase().GetExpr() * 1000000);

				// Calculate the time difference between the last decoded frame and the current decoded frame.
				int64_t hole_time_us = curr_decoded_frame_time_us - (last_decoded_frame_time_us + last_decoded_frame_duration_us);
				int64_t hole_time_tb = (int64_t)(floor((double)hole_time_us / input_track->GetTimeBase().GetExpr() / 1000000));

				int64_t duration_per_frame = 0;
				switch (input_track->GetMediaType())
				{
					case cmn::MediaType::Video:
						duration_per_frame = input_track->GetTimeBase().GetTimescale() / input_track->GetFrameRate();
						break;
					case cmn::MediaType::Audio:
						duration_per_frame = decoded_frame->GetNbSamples();
						break;
					default:
						break;
				}

				// If the time difference is greater than 0, it means that there is a hole between with the last frame and the current frame.
				if (hole_time_tb >= duration_per_frame)
				{
					int64_t start_pts = decoded_frame->GetPts() - hole_time_tb;
					int64_t end_pts = decoded_frame->GetPts();
					int32_t needed_frames = hole_time_tb / duration_per_frame;
					int64_t reamiain_pts = hole_time_tb - (needed_frames * duration_per_frame);

					logtd("%s Create filler frame because time diffrence from last frame. Type(%s), needed(%d), last_pts(%lld), curr_pts(%lld), hole_time(%lld), hole_time_tb(%lld), frame_duration(%lld), remain_pts(%lld), start_pts(%lld), end_pts(%lld)",
						  _log_prefix.CStr(), cmn::GetMediaTypeString(input_track->GetMediaType()).CStr(), needed_frames, last_decoded_frame_time_us, curr_decoded_frame_time_us, hole_time_us, hole_time_tb, duration_per_frame, reamiain_pts, start_pts, end_pts);

					for (int64_t filler_pts = start_pts; filler_pts < end_pts; filler_pts += duration_per_frame)
					{
						std::shared_ptr<MediaFrame> clone_frame = decoded_frame->CloneFrame(true);
						if (!clone_frame)
						{
							continue;
						}
						clone_frame->SetPts(filler_pts);
						clone_frame->SetDuration(duration_per_frame);

						if (input_track->GetMediaType() == cmn::MediaType::Audio)
						{
							if (end_pts - filler_pts < duration_per_frame)
							{
								int32_t remain_samples = end_pts - filler_pts;
								clone_frame->SetDuration(remain_samples);

								// There is no problem making the Samples smaller since the cloned frame is larger than the remaining samples.
								// To do this properly, It need to reallocate audio buffers of MediaFrame.
								clone_frame->SetNbSamples(remain_samples);
							}
							clone_frame->FillZeroData();
						}

						SpreadToFilters(decoder_id, clone_frame);
						// logtd("%s Create filler frame. Type(%s), %s", _log_prefix.CStr(), cmn::GetMediaTypeString(input_track->GetMediaType()).CStr(), clone_frame->GetInfoString().CStr());
					}
				}
			}
#endif // End of Filler Frame Generation	

			[[fallthrough]];
		}
		
		case TranscodeResult::DataReady: {
			

			// The last decoded frame is kept and used as a filling frame in the blank section.
			SetLastDecodedFrame(decoder_id, decoded_frame);

			// Send Decoded Frame to Filter
			SpreadToFilters(decoder_id, decoded_frame);
		}
		break;
		default:
			// An error occurred
			// There is no frame to process
			break;
	}
}

void TranscoderStream::SetLastDecodedFrame(MediaTrackId decoder_id, std::shared_ptr<MediaFrame> &decoded_frame)
{
	auto input_track = GetInputTrack(decoder_id);

	auto scale_factor = input_track->GetTimeBase().GetExpr() * 1000000.0;
	_last_decoded_frame_pts[decoder_id] = static_cast<int64_t>(decoded_frame->GetPts() * scale_factor);
	_last_decoded_frame_duration[decoder_id] = static_cast<int64_t>(decoded_frame->GetDuration() * scale_factor);

	_last_decoded_frames[decoder_id] = decoded_frame->CloneFrame();
}

std::shared_ptr<MediaFrame> TranscoderStream::GetLastDecodedFrame(MediaTrackId decoder_id)
{
	if (_last_decoded_frames.find(decoder_id) != _last_decoded_frames.end())
	{
		auto frame = _last_decoded_frames[decoder_id]->CloneFrame();
		frame->SetTrackId(decoder_id);
		return frame;
	}

	return nullptr;
}

std::shared_ptr<MediaTrack> TranscoderStream::GetInputTrackOfFilter(MediaTrackId decoder_id)
{
	auto decoder_to_filter_map_it = _link_decoder_to_filters.find(decoder_id);
	if (decoder_to_filter_map_it == _link_decoder_to_filters.end())
	{
		return nullptr;
	}

	auto filter_ids = decoder_to_filter_map_it->second;
	if (filter_ids.size() == 0)
	{
		return nullptr;
	}

	auto filter = GetFilter(filter_ids[0]);
	if (filter == nullptr)
	{
		return nullptr;
	}

	return filter->GetInputTrack();
}

TranscodeResult TranscoderStream::FilterFrame(MediaTrackId filter_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	auto filter = GetFilter(filter_id);
	if (filter == nullptr)
	{
		return TranscodeResult::NoData;
	}

	if (filter->SendBuffer(std::move(decoded_frame)) == false)
	{
		return TranscodeResult::DataError;
	}

	return TranscodeResult::DataReady;
}

void TranscoderStream::OnFilteredFrame(MediaTrackId filter_id, std::shared_ptr<MediaFrame> filtered_frame)
{
	if(_is_updating == true)
	{
		logtd("%s Current state is updating format. suspend filted frame. Filter(%d)", _log_prefix.CStr(), filter_id);
		return;
	}

	filtered_frame->SetTrackId(filter_id);

	PreEncodeFilterFrame(std::move(filtered_frame));
}

TranscodeResult TranscoderStream::PreEncodeFilterFrame(std::shared_ptr<MediaFrame> frame)
{
	auto filter_id = frame->GetTrackId();

	// Get Encoder ID form Filter ID
	auto filter_to_encoder_it = _link_filter_to_encoder.find(filter_id);
	if (filter_to_encoder_it == _link_filter_to_encoder.end())
	{
		return TranscodeResult::NoData;
	}
	auto encoder_id = filter_to_encoder_it->second;

	// Get EncoderSet
	auto encoder_set = GetEncoder(encoder_id);
	if(encoder_set.has_value() == false)
	{
		return TranscodeResult::NoData;
	}

	// If the encoder has a pre-encode filter, it is passed to the filter.
	auto encode_filter = encoder_set->first;
	if (encode_filter != nullptr)
	{
		encode_filter->SendBuffer(std::move(frame));

		return TranscodeResult::DataReady;
	}

	OnPreEncodeFilteredFrame(encoder_id, std::move(frame));
	
	return TranscodeResult::DataReady;
}

void TranscoderStream::OnPreEncodeFilteredFrame(MediaTrackId encoder_id, std::shared_ptr<MediaFrame> filtered_frame)
{
	if(_is_updating == true)
	{
		logtd("%s Current state is updating format. suspend filtered frame. Encoder(%d)", _log_prefix.CStr(), encoder_id);
		return;
	}

	filtered_frame->SetTrackId(encoder_id);

	EncodeFrame(std::move(filtered_frame));
}

TranscodeResult TranscoderStream::EncodeFrame(std::shared_ptr<const MediaFrame> frame)
{
	auto encoder_id = frame->GetTrackId();

	// Get Encoder
	auto encoder_set = GetEncoder(encoder_id);
	if (encoder_set.has_value() == false)
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_set->second;
	if (encoder == nullptr)
	{
		return TranscodeResult::NoData;
	}

	encoder->SendBuffer(std::move(frame));
	return TranscodeResult::DataReady;
}

// Callback is called from the encoder for packets that have been encoded.
void TranscoderStream::OnEncodedPacket(MediaTrackId encoder_id, std::shared_ptr<MediaPacket> encoded_packet)
{
	if (encoded_packet == nullptr)
	{
		return;
	}

	auto it = _link_encoder_to_outputs.find(encoder_id);
	if (it == _link_encoder_to_outputs.end())
	{
		return;
	}
	auto output_tracks = it->second;


	// The encoded packet is used in multiple tracks. 
	int32_t used_count = output_tracks.size();
	for (auto &[output_stream, output_track_id] : output_tracks)
	{
		std::shared_ptr<MediaPacket> packet = nullptr;
		
		// If the packet is used in multiple tracks, it is cloned. 
		packet = (used_count == 1) ? encoded_packet : encoded_packet->ClonePacket();
		if (packet == nullptr)
		{
			logte("%s Could not clone packet. Encoder(%d)", _log_prefix.CStr(), encoder_id);
			continue;
		}

		packet->SetTrackId(output_track_id);

		// Send the packet to MediaRouter
		SendFrame(output_stream, std::move(packet));

		used_count--;
	}
}

void TranscoderStream::UpdateMsidOfOutputStreams(uint32_t msid)
{
	// Update info::Stream().msid of all output streams
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)
		
		output_stream->SetMsid(msid);
	}
}

void TranscoderStream::SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet)
{
	packet->SetMsid(stream->GetMsid());

	// Send the packet to MediaRouter
	bool ret = _parent->SendFrame(stream, std::move(packet));
	if (ret == false)
	{
		logtw("%s Could not send frame to mediarouter. Stream(%s(%u)), OutputTrack(%u)",
			  _log_prefix.CStr(), stream->GetName().CStr(), stream->GetId(), packet->GetTrackId());
	}
}

void TranscoderStream::SpreadToFilters(MediaTrackId decoder_id, std::shared_ptr<MediaFrame> frame)
{
	auto filters = _link_decoder_to_filters.find(decoder_id);
	if (filters == _link_decoder_to_filters.end())
	{
		logtw("%s Could not found filter", _log_prefix.CStr());

		return;
	}
	
	auto filter_ids = filters->second;

	for (auto &filter_id : filter_ids)
	{
		auto frame_clone = frame->CloneFrame(true);
		if (frame_clone == nullptr)
		{
			logte("%s Failed to clone frame", _log_prefix.CStr());

			continue;
		}

		FilterFrame(filter_id, std::move(frame_clone));
	}
}


void TranscoderStream::NotifyCreateStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		if (_parent->CreateStream(output_stream) == false)
		{
			logtw("%s Could not create stream. [%s/%s(%u)]",
				  _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
		}
	}
}

void TranscoderStream::NotifyDeleteStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		if (_parent->DeleteStream(output_stream) == false)
		{
			logtw("%s Could not delete stream. %s/%s(%u)",
				  _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
		}
	}

	_output_streams.clear();
}

void TranscoderStream::NotifyUpdateStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		if (_parent->UpdateStream(output_stream) == false)
		{
			logtw("%s Could not update stream. %s/%s(%u)",
				  _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
		}
	}
}
