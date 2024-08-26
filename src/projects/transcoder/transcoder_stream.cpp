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

	RemoveAllComponents();

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

	StoreInputTrackSnapshot(_input_stream);

	return true;
}

bool TranscoderStream::UpdateInternal(const std::shared_ptr<info::Stream> &stream)
{
	// Check if smooth stream transition is possible
	// [Rule]
	// - The number of tracks per media type should not exceed one.
	// - The input track should not change.
	if (IsAvailableSmoothTransition(stream) == true)
	{
		logti("%s This stream will be a smooth transition", _log_prefix.CStr());
		RemoveDecoders();
		RemoveFilters();
		
		CreateDecoders();
	}
	else
	{
		logti("%s This stream does not support smooth transitions. renew the all components", _log_prefix.CStr());
		RemoveAllComponents();
		
		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());
		NotifyUpdateStreams();

		StoreInputTrackSnapshot(stream);
	}

	return true;
}

void TranscoderStream::RemoveAllComponents()
{
	// Stop all decoder
	RemoveDecoders();
	
	// Stop all filters
	RemoveFilters();
	
	// Stop all encoders
	RemoveEncoders();
}

void TranscoderStream::RemoveDecoders()
{
	std::lock_guard<std::shared_mutex> decoder_lock(_decoder_map_mutex);
	
	for (auto &it : _decoders)
	{
		auto object = it.second;
		object->Stop();
		object.reset();
	}
	_decoders.clear();
}

void TranscoderStream::RemoveFilters()
{
	std::lock_guard<std::shared_mutex> filter_lock(_filter_map_mutex);
	
	for (auto &it : _filters)
	{
		auto object = it.second;
		object->Stop();
		object.reset();
	}
	_filters.clear();
}

void TranscoderStream::RemoveEncoders()
{
	std::lock_guard<std::shared_mutex> encoder_lock(_encoder_map_mutex);
	for (auto &iter : _encoders)
	{
		auto object = iter.second;
		object->Stop();
		object.reset();
	}
	_encoders.clear();
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

bool TranscoderStream::IsAvailableSmoothTransition(const std::shared_ptr<info::Stream> &input_stream)
{
	// #1. The number of tracks per media type should not exceed one.
	int32_t track_count_per_mediatype[(uint32_t)cmn::MediaType::Nb] = {0};
	for (const auto &[track_id, track] : input_stream->GetTracks())
	{
		UNUSED_VARIABLE(track_id)
		
		if ((++track_count_per_mediatype[(uint32_t)track->GetMediaType()]) > 1)
		{
			// Could not support smooth transition. because, number of tracks per media type exceed one.
			logtw("%s Smooth transitions are not possible because the number of tracks per media type exceeds one.", _log_prefix.CStr());
			return false;
		}
	}

	// #2. Check if the number and type of original tracks are different.
	if(IsEqualCountAndMediaTypeOfMediaTracks(input_stream->GetTracks(), GetInputTrackSnapshot()) == false)
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

		DecodePacket(std::move(packet));
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
	
		DecodePacket(std::move(media_packet));
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
	auto cfg_output_profile = GetOutputProfilesCfg();
	auto cfg_output_profile_list = cfg_output_profile->GetOutputProfileList();
	
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

	// Playlist
	bool is_parsed = false;
	auto cfg_playlists = cfg_output_profile.GetPlaylists(&is_parsed);
	if (is_parsed)
	{
		for (const auto &cfg_playlist : cfg_playlists)
		{
			auto playlist_info = cfg_playlist.GetPlaylistInfo();
			output_stream->AddPlaylist(playlist_info);
		}
	}

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
						logtw("Failed to create media tracks. Encoding options need to be checked. track_id(%d)", input_track_id);
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
						logtw("Failed to create media tracks. Encoding options need to be checked. track_id(%d)", input_track_id);
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
						logtw("Failed to create media tracks. Encoding options need to be checked. track_id(%d)", input_track_id);
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
						logtw("Failed to create media tracks. Encoding options need to be checked. track_id(%d)", input_track_id);
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

	return output_stream;
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
			// Bypass Flow: InputTrack -> OutputTrack
			if (output_track->IsBypass() == true)
			{
				_link_input_to_outputs[input_track_id].push_back(make_pair(output_stream, output_track->GetId()));
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
				_link_encoder_to_outputs[encoder_id].push_back(make_pair(output_stream, output_track->GetId()));
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
	std::lock_guard<std::shared_mutex> decoder_lock(_decoder_map_mutex);

	// If there is an existing decoder, do not create decoder
	if (_decoders.find(decoder_id) != _decoders.end())
	{
		logtw("%s Decoder already exists. InputTrack(%d) > Decoder(%d)", _log_prefix.CStr(), input_track->GetId(), decoder_id);

		return true;
	}

	// Get HWAccels configuration
	auto cfg_hwaccels = GetOutputProfilesCfg()->GetHWAccels();

	// Get a list of available decoder candidates.
	// TODO: GetOutputProfilesCfg()->IsHardwareAcceleration() is deprecated. It will be deleted soon.
	auto candidates = TranscodeDecoder::GetCandidates(cfg_hwaccels.GetDecoder().IsEnable() || GetOutputProfilesCfg()->IsHardwareAcceleration(), cfg_hwaccels.GetDecoder().GetModules(), input_track);
	if(candidates == nullptr)
	{
		logte("%s Decoder candidates are not found. InputTrack(%u)", _log_prefix.CStr(), input_track->GetId());
		return false;
	}

	// Create a decoder
	auto decoder = TranscodeDecoder::Create(
		decoder_id,
		*(input_stream),
		input_track,
		candidates,
		bind(&TranscoderStream::OnDecodedFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	if (decoder == nullptr)
	{
		logte("%s Decoder allocation failed.  InputTrack(%u) > Decoder(%u)", _log_prefix.CStr(), input_track->GetId(), decoder_id);
		return false;
	}

	_decoders[decoder_id] = std::move(decoder);

	logtd("%s Created decoder. InputTrack(%u) > Decoder(%u)", _log_prefix.CStr(), input_track->GetId(), decoder_id);

	return true;
}

int32_t TranscoderStream::CreateEncoders(MediaFrame *buffer)
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

			// Set the sample format and color space supported by the encoder to the output track.
			// These values are used in the Resampler/Rescaler filter.
			if (output_track->GetMediaType() == cmn::MediaType::Video)
			{
				auto encoder = _encoders[encoder_id];

				output_track->SetColorspace(encoder->GetSupportedFormat());
			}
			else if (output_track->GetMediaType() == cmn::MediaType::Audio)
			{
				auto encoder = _encoders[encoder_id];

				output_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(encoder->GetSupportedFormat()));
			}

			created++;
		}
	}

	return created;
}

bool TranscoderStream::CreateEncoder(MediaTrackId encoder_id, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	std::lock_guard<std::shared_mutex> encoder_lock(_encoder_map_mutex);

	if (_encoders.find(encoder_id) != _encoders.end())
	{
		logtd("%s The encoder has already created. Encoder(%d) > OutputTrack(%d)", _log_prefix.CStr(), encoder_id, output_track->GetId());
		return true;
	}

	// Get HWAccels configuration
	auto cfg_hwaccels = GetOutputProfilesCfg()->GetHWAccels();

	// Get a list of available encoder candidates.
	// TODO: GetOutputProfilesCfg()->IsHardwareAcceleration() is deprecated. It will be deleted soon.
	auto candidates = TranscodeEncoder::GetCandidates(cfg_hwaccels.GetEncoder().IsEnable() || GetOutputProfilesCfg()->IsHardwareAcceleration(), cfg_hwaccels.GetEncoder().GetModules(), output_track);
	if(candidates == nullptr)
	{
		logte("%s Decoder candidates are not found. InputTrack(%d)", _log_prefix.CStr(), output_track->GetId());
		return false;
	}

	auto encoder = TranscodeEncoder::Create(encoder_id, *output_stream, output_track, candidates, bind(&TranscoderStream::OnEncodedPacket, this, std::placeholders::_1, std::placeholders::_2));
	if (encoder == nullptr)
	{
		return false;
	}

	_encoders[encoder_id] = std::move(encoder);

	logtd("%s Created encoder. Encoder(%d) > OutputTrack(%d)", _log_prefix.CStr(), encoder_id, output_track->GetId());
	return true;
}


int32_t TranscoderStream::CreateFilters(MediaFrame *buffer)
{
	int32_t created = 0;

	MediaTrackId decoder_id = buffer->GetTrackId();

	// 1. Get Input Track of Decoder
	auto input_track = _decoders[decoder_id]->GetRefTrack();

	auto decoder_to_filters_it = _link_decoder_to_filters.find(decoder_id);
	if (decoder_to_filters_it == _link_decoder_to_filters.end())
	{
		logtw("%s Could not found filter list related to decoder", _log_prefix.CStr());

		return created;
	}
	auto filter_ids = decoder_to_filters_it->second;

	// 2. Get Output Track of Encoders
	for (auto &filter_id : filter_ids)
	{
		MediaTrackId encoder_id = _link_filter_to_encoder[filter_id];
		if (_encoders.find(encoder_id) == _encoders.end())
		{
			logte("%s Filter creation failed because the encoder could not be found. EncoderId(%d), FilterId(%d)", _log_prefix.CStr(), encoder_id, filter_id);
			continue;
		}

		auto output_track = _encoders[encoder_id]->GetRefTrack();

		if(CreateFilter(filter_id, input_track, output_track) == false)
		{
			continue;
		}
		created++;
	}

	return created;
}

bool TranscoderStream::CreateFilter(MediaTrackId filter_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<MediaTrack> output_track)
{
	std::lock_guard<std::shared_mutex> filter_lock(_filter_map_mutex);

	if (_filters.find(filter_id) != _filters.end())
	{
		logtd("%s The filter has already created. FilterId(%d)", _log_prefix.CStr(), filter_id);

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

	auto filter = std::make_shared<TranscodeFilter>();
	if (filter->Configure(filter_id, input_stream, input_track, output_stream, output_track, bind(&TranscoderStream::OnFilteredFrame, this, std::placeholders::_1, std::placeholders::_2)) != true)
	{
		logte("%s Failed to create filter. Filter(%d)", _log_prefix.CStr(), filter_id);
		return false;
	}

	_filters[filter_id] = filter;

	logtd("%s Created Filter. FilterId(%d)", _log_prefix.CStr(), filter_id);

	return true;
}


// Function called when codec information is extracted or changed from the decoder
void TranscoderStream::ChangeOutputFormat(MediaFrame *buffer)
{
	logtd("%s Changed output format. InputTrack(%u)", _log_prefix.CStr(), buffer->GetTrackId());

	std::lock_guard<std::shared_mutex> lock(_format_change_mutex);

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

		SetState(State::ERROR);
		return;
	}

	// Create an filter. If there is an existing filter, reuse it
	if(CreateFilters(buffer) == 0)
	{
		logtw("%s No filters have been created. InputTrack(%u)", _log_prefix.CStr(), buffer->GetTrackId());
		
		SetState(State::ERROR);
		return;
	}
}

// Information of the input track is updated by the decoded frame
void TranscoderStream::UpdateInputTrack(MediaFrame *buffer)
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
void TranscoderStream::UpdateOutputTrack(MediaFrame *buffer)
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

			logtd("%s Updated output track. InputTrack(%u), OutputTrack(%u)", _log_prefix.CStr(), input_track_id, output_track->GetId());

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


void TranscoderStream::DecodePacket(const std::shared_ptr<MediaPacket> &packet)
{
	MediaTrackId input_track_id = packet->GetTrackId();

	// 1. Packet to Output Stream (bypass)
	auto output_streams = _link_input_to_outputs.find(input_track_id);
	if (output_streams != _link_input_to_outputs.end())
	{
		auto &output_tracks = output_streams->second;

		for (auto &[output_stream, output_track_id] : output_tracks)
		{
			auto input_track = _input_stream->GetTrack(input_track_id);
			if (input_track == nullptr)
			{
				logte("%s Could not found input track. InputTrack(%d)", _log_prefix.CStr(), input_track_id);
				continue;
			}

			auto output_track = output_stream->GetTrack(output_track_id);
			if (output_track == nullptr)
			{
				logte("%s Could not found output track. OutputTrack(%d)", _log_prefix.CStr(), output_track_id);
				continue;
			}

			double scale = input_track->GetTimeBase().GetExpr() / output_track->GetTimeBase().GetExpr();

			// Clone the packet and send it to the output stream.
			std::shared_ptr<MediaPacket> clone_packet = nullptr;
			
			if (packet->GetBitstreamFormat() == cmn::BitstreamFormat::OVEN_EVENT)
			{
				auto event_packet = std::static_pointer_cast<MediaEvent>(packet);
				clone_packet = event_packet->Clone();
			}
			else 
			{
				clone_packet = packet->ClonePacket();
			}
			
			clone_packet->SetTrackId(output_track_id);
			clone_packet->SetPts((int64_t)((double)clone_packet->GetPts() * scale));
			clone_packet->SetDts((int64_t)((double)clone_packet->GetDts() * scale));

			SendFrame(output_stream, std::move(clone_packet));
		}
	}


	// 2. Packet to Decoder (transcoding)
	auto input_to_decoder_it = _link_input_to_decoder.find(input_track_id);
	if (input_to_decoder_it == _link_input_to_decoder.end())
	{
		return;
	}
	auto decoder_id = input_to_decoder_it->second;

	std::shared_lock<std::shared_mutex> lock(_decoder_map_mutex);

	auto decoder_it = _decoders.find(decoder_id);
	if (decoder_it == _decoders.end())
	{
		return;
	}

	auto decoder = decoder_it->second;
	decoder->SendBuffer(std::move(packet));
}

void TranscoderStream::OnDecodedFrame(TranscodeResult result, MediaTrackId decoder_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	if (decoded_frame == nullptr)
	{
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
			_last_decoded_frame_pts[decoder_id] = last_frame->GetPts() * filter_expr * 1000000;

			logtd("%s Create filler frame because there is no decoding frame. Type(%s), Decoder(%u), FillerFrames(%d)"
				, _log_prefix.CStr(), cmn::GetMediaTypeString(input_track->GetMediaType()).CStr(), decoder_id, 1);

			// Send Temporary Frame to Filter
			SpreadToFilters(decoder_id, last_frame);
#endif // End of Filler Frame Generation
		}
		break;

		// It indicates output format is changed
		case TranscodeResult::FormatChanged: {

			// Re-create filter and encoder using the format of decoded frame
			ChangeOutputFormat(decoded_frame.get());

 #if FILLER_ENABLED
			///////////////////////////////////////////////////////////////////
			// Generate a filler frame (Part 2). * Using latest decoded frame
			///////////////////////////////////////////////////////////////////
			// - It is mainly used in Persistent Stream.
			// - When the input stream is changed, an empty section occurs in sequential frames. There is a problem with the A/V sync in the player.
			//   If there is a section where the frame is empty, may be out of sync in the player.
			//   Therefore, the filler frame is inserted in the hole of the sequential frame.
			auto it = _last_decoded_frame_pts.find(decoder_id);
			if(it != _last_decoded_frame_pts.end())
			{
				auto input_track = GetInputTrack(decoder_id);
				if(!input_track)
				{
					logte("Could not found input track. Decoder(%d)", decoder_id);
					return;
				}

				int64_t frame_hole_time_us = (int64_t)((double)decoded_frame->GetPts() * input_track->GetTimeBase().GetExpr() * 1000000) - (int64_t)it->second;
				if(frame_hole_time_us > 0)
				{
					int32_t number_of_filler_frames_needed = 0;

					switch(input_track->GetMediaType())
					{
						case cmn::MediaType::Video:
							number_of_filler_frames_needed = (int32_t)(((double)frame_hole_time_us/1000000) / (1.0f / input_track->GetFrameRate()));
							break;
						case cmn::MediaType::Audio:
							number_of_filler_frames_needed = (int32_t)(((double)frame_hole_time_us/1000000) / (input_track->GetTimeBase().GetExpr() * (double)decoded_frame->GetNbSamples()));
							break;
						default:
							break;
					}

					if(number_of_filler_frames_needed > 0)					
					{
						logtd("%s Create filler frame because time diffrence from last frame. Type(%s), Decoder(%u), FillerFrames(%d)"
							,_log_prefix.CStr(), cmn::GetMediaTypeString(input_track->GetMediaType()).CStr(), decoder_id, number_of_filler_frames_needed);

						int64_t frame_hole_time_tb 	= (int64_t)((double)frame_hole_time_us / input_track->GetTimeBase().GetExpr() / 1000000);
						int64_t frame_duration_avg 	= frame_hole_time_tb / number_of_filler_frames_needed;
						int64_t start_pts 			= decoded_frame->GetPts() - frame_hole_time_tb + frame_duration_avg;
						int64_t end_pts 			= decoded_frame->GetPts();

						for (int64_t filler_pts = start_pts; filler_pts < end_pts; filler_pts += frame_duration_avg)
						{
							auto clone_frame = decoded_frame->CloneFrame();
							clone_frame->SetPts(filler_pts);

							// Fill the silence in the audio frame.
							if (input_track->GetMediaType() == cmn::MediaType::Audio)
							{
								clone_frame->FillZeroData();
							}

							SpreadToFilters(decoder_id, clone_frame);
						}
					}
				}
			} 
#endif // End of Filler Frame Generation	

			[[fallthrough]];
		}
		
		case TranscodeResult::DataReady: {

			auto input_track = GetInputTrack(decoder_id);

			// Record the timestamp of the last decoded frame. managed by microseconds.
			_last_decoded_frame_pts[decoder_id] = decoded_frame->GetPts() * input_track->GetTimeBase().GetExpr() * 1000000;

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

	std::shared_lock<std::shared_mutex> lock(_filter_map_mutex);

	auto it = _filters.find(filter_ids[0]);
	if (it == _filters.end())
	{
		return nullptr;
	}
	auto filter = it->second;
	
	return filter->GetInputTrack();
}

TranscodeResult TranscoderStream::FilterFrame(MediaTrackId filter_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	std::shared_lock<std::shared_mutex> lock(_filter_map_mutex);

	auto filter_it = _filters.find(filter_id);
	if (filter_it == _filters.end())
	{
		return TranscodeResult::NoData;
	}

	auto filter = filter_it->second.get();
	if (filter->SendBuffer(std::move(decoded_frame)) == false)
	{
		return TranscodeResult::DataError;
	}

	return TranscodeResult::DataReady;
}

void TranscoderStream::OnFilteredFrame(MediaTrackId filter_id, std::shared_ptr<MediaFrame> filtered_frame)
{
	filtered_frame->SetTrackId(filter_id);

	EncodeFrame(std::move(filtered_frame));
}

TranscodeResult TranscoderStream::EncodeFrame(std::shared_ptr<const MediaFrame> frame)
{
	auto filter_id = frame->GetTrackId();

	// Get Encoder ID
	auto filter_to_encoder_it = _link_filter_to_encoder.find(filter_id);
	if(filter_to_encoder_it == _link_filter_to_encoder.end())
	{
		return TranscodeResult::NoData;
	}
	auto encoder_id = filter_to_encoder_it->second;

	// Get Encoder 
	std::shared_lock<std::shared_mutex> lock(_encoder_map_mutex);
	
	auto encoder_map_it = _encoders.find(encoder_id);
	if (encoder_map_it == _encoders.end())
	{
		return TranscodeResult::NoData;
	}
	auto encoder = encoder_map_it->second.get();

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

	// Explore if output tracks exist to send encoded packets
	auto encoder_to_outputs_it = _link_encoder_to_outputs.find(encoder_id);
	if (encoder_to_outputs_it == _link_encoder_to_outputs.end())
	{
		return;
	}
	auto output_tracks = encoder_to_outputs_it->second;

	// If a track exists to output, copy the encoded packet and send it to that track.
	for (auto &[output_stream, output_track_id] : output_tracks)
	{
		std::shared_ptr<MediaPacket> clone_packet = nullptr;
		
		if (encoded_packet->GetBitstreamFormat() == cmn::BitstreamFormat::OVEN_EVENT)
		{
			auto event_packet = std::static_pointer_cast<MediaEvent>(encoded_packet);
			clone_packet = event_packet->Clone();
		}
		else 
		{
			clone_packet = encoded_packet->ClonePacket();
		}
		clone_packet->SetTrackId(output_track_id);

		// Send the packet to MediaRouter
		SendFrame(output_stream, std::move(clone_packet));
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

	bool ret = _parent->SendFrame(stream, std::move(packet));
	if (ret == false)
	{
		logtw("%s Could not send frame to mediarouter. Stream(%s(%u)), OutputTrack(%u)", _log_prefix.CStr(), stream->GetName().CStr(), stream->GetId(), packet->GetTrackId());
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
			logtw("%s Could not create stream. [%s/%s(%u)]", _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
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
			logtw("%s Could not delete stream. %s/%s(%u)", _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
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
			logtw("%s Could not update stream. %s/%s(%u)", _log_prefix.CStr(), _application_info.GetVHostAppName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
		}
	}
}
