//==============================================================================
//
//  TranscoderStream
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_stream.h"

#include <config/config_manager.h>

#include "transcoder_application.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 100
#define GENERATE_FILLER_FRAME true
#define UNUSED_VARIABLE(var) (void)var;

TranscoderStream::TranscoderStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _parent(parent), _application_info(application_info), _input_stream(stream)
{
	_log_prefix = ov::String::FormatString("[%s/%s(%u)]", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	logtd("%s Trying to create transcode stream", _log_prefix.CStr());
}

TranscoderStream::~TranscoderStream()
{
	Stop();
}

info::stream_id_t TranscoderStream::GetStreamId()
{
	if (_input_stream != nullptr)
		return _input_stream->GetId();

	return 0;
}

bool TranscoderStream::Start()
{
	_is_stopped = false;

	logti("%s Transcoder stream has been started", _log_prefix.CStr());

	return true;
}

bool TranscoderStream::Stop()
{
	if(_is_stopped == true)
	{
		return true;
	}

	logtd("%s Wait for terminated trancode stream thread", _log_prefix.CStr());

	RemoveAllComponents();

	// Notify to delete the stream created on the MediaRouter
	NotifyDeleteStreams();

	// Delete all composite of componetns
	_link_input_to_outputs.clear();
	_link_input_to_decoder.clear();
	_link_decoder_to_filters.clear();
	_link_filter_to_encoder.clear();
	_link_encoder_to_outputs.clear();

	// Delete all last decoded frame information
	_last_decoded_frame_pts.clear();
	_last_decoded_frames.clear();

	// Delete all output streams information
	_output_streams.clear();

	_is_stopped = true;

	logti("%s Transcoder stream has been stopped", _log_prefix.CStr());

	return true;
}


bool TranscoderStream::Prepare(const std::shared_ptr<info::Stream> &stream)
{
	if(StartInternal() == false)
	{
		return false;
	}

	logti("%s Transcoder stream has been prepared", _log_prefix.CStr());

	return true;
}

bool TranscoderStream::StartInternal()
{
	if (_link_input_to_outputs.size() > 0 || _link_encoder_to_outputs.size() > 0)
	{
		logtd("%s This stream has already been created", _log_prefix.CStr());
		return true;
	}

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

	if (BuildComposite() == 0)
	{
		logte("No components generated");
		return false;
	}

	if (CreateDecoders() == 0)
	{
		logti("No decoder generated");
	}

	// Notify to create a new stream on the media router.
	NotifyCreateStreams();

	return true;
}

bool TranscoderStream::Update(const std::shared_ptr<info::Stream> &stream)
{
	// Check if smooth stream transition is possible
	// [Rule]
	// - The number of tracks per media type should not exceed one.
	// - The number of tracks of the stream to be changed must be the same.
	if (IsAvailableSmoothTransitionStream(stream) == true)
	{
		logti("%s This stream will be a smooth transition", _log_prefix.CStr());

		RemoveDecoders();

		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());
	}
	else
	{
		logti("%s This stream does not support smooth transitions. renew the encoder", _log_prefix.CStr());

		RemoveAllComponents();

		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());

		NotifyUpdateStreams();
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

bool TranscoderStream::IsAvailableSmoothTransitionStream(const std::shared_ptr<info::Stream> &stream)
{
	int32_t track_count_per_mediatype[(uint32_t)cmn::MediaType::Nb] = {0};

	// Rule 1: The number of tracks per media type should not exceed one.
	for (const auto &[track_id, track] : stream->GetTracks())
	{
		UNUSED_VARIABLE(track_id)
		
		if ((++track_count_per_mediatype[(uint32_t)track->GetMediaType()]) > 1)
		{
			// Could not support smooth transition. because, number of tracks per media type exceed one.
			return false;
		}
	}

	// Rule 2 : The number of tracks of the stream to be changed must be the same.
	//  - Not applied yet.

	return true;
}


bool TranscoderStream::Push(std::shared_ptr<MediaPacket> packet)
{
	DecodePacket(std::move(packet));

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

	logti("%s dynamic output stream has been created. [%s/%s(%u)]",
		  _log_prefix.CStr(),
		  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

	created_count++;

	return created_count;
}

int32_t TranscoderStream::CreateOutputStreams()
{
	int32_t created_count = 0;

	// Get [application->Streams] list of application configuration.
	auto &cfg_output_profile_list = _application_info.GetConfig().GetOutputProfileList();

	// Get the output  to make the output stream
	for (const auto &cfg_output_profile : cfg_output_profile_list)
	{
		auto output_stream = CreateOutputStream(cfg_output_profile);
		if (output_stream == nullptr)
		{
			logte("Cloud not create output stream");
			continue;
		}

		_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream));

		logti("%s Output stream has been created. [%s/%s(%u)]", _log_prefix.CStr(), _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

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

	auto stream = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);
	if (stream == nullptr)
	{
		return nullptr;
	}

	// It helps modules to recognize origin stream from provider
	stream->LinkInputStream(_input_stream);
	stream->SetMediaSource(_input_stream->GetUUID());

	// Create a new stream name.
	auto name = cfg_output_profile.GetOutputStreamName();
	if (::strstr(name.CStr(), "${OriginStreamName}") != nullptr)
	{
		name = name.Replace("${OriginStreamName}", _input_stream->GetName());
	}
	stream->SetName(name);

	// Playlist
	bool is_parsed = false;
	auto cfg_playlists = cfg_output_profile.GetPlaylists(&is_parsed);
	if (is_parsed)
	{
		for (const auto &cfg_playlist : cfg_playlists)
		{
			auto playlist = std::make_shared<info::Playlist>(cfg_playlist.GetName(), cfg_playlist.GetFileName());
			playlist->SetWebRtcAutoAbr(cfg_playlist.GetOptions().IsWebRtcAutoAbr());
			playlist->SetHlsChunklistPathDepth(cfg_playlist.GetOptions().GetHlsChunklistPathDepth());

			for (const auto &cfg_rendition : cfg_playlist.GetRenditions())
			{
				playlist->AddRendition(std::make_shared<info::Rendition>(cfg_rendition.GetName(), cfg_rendition.GetVideoName(), cfg_rendition.GetAudioName()));
			}

			stream->AddPlaylist(playlist);
		}
	}

	// Output Track
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

					stream->AddTrack(output_track);

					auto profile_sign = GetIdentifiedForVideoProfile(input_track_id, profile);
					AddComposite(profile_sign, _input_stream, input_track, stream, output_track);
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

					stream->AddTrack(output_track);

					auto profile_sign = GetIdentifiedForImageProfile(input_track_id, profile);
					AddComposite(profile_sign, _input_stream, input_track, stream, output_track);
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

					stream->AddTrack(output_track);

					auto profile_sign = GetIdentifiedForAudioProfile(input_track_id, profile);
					AddComposite(profile_sign, _input_stream, input_track, stream, output_track);
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

					stream->AddTrack(output_track);

					auto profile_sign = GetIdentifiedForDataProfile(input_track_id);
					AddComposite(profile_sign, _input_stream, input_track, stream, output_track);
			}
			break;
			default: {
				logtw("Unsupported media type of input track. type(%d)", input_track->GetMediaType());
				continue;
			}
		}
	}	

	return stream;
}

int32_t TranscoderStream::BuildComposite()
{
	int32_t created_count = 0;


	for (auto &[key, composite] : _composite_map)
	{
		UNUSED_VARIABLE(key)

		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			auto input_track = composite->GetInputTrack();

			auto input_track_id = input_track->GetId();
			auto decoder_id = input_track->GetId();

			auto filter_id = composite->GetId();
			auto encoder_id = composite->GetId();

			auto output_track_id = output_track->GetId();

			// Bypass Flow: InputTrack -> OutputTrack
			if (output_track->IsBypass() == true)
			{
				_link_input_to_outputs[input_track_id].push_back(make_pair(output_stream, output_track_id));
			}
			// Transcoding Flow: InputTrack -> Decoder -> Filter -> Encoder -> OutputTrack
			else
			{
				// Decoding: InputTrack(1) -> Decoder(1)
				_link_input_to_decoder[input_track_id] = decoder_id;

				// Rescale/Resample Filtering: Decoder(1) -> Filter (N)
				if (std::find(_link_decoder_to_filters[decoder_id].begin(), _link_decoder_to_filters[decoder_id].end(), filter_id) == _link_decoder_to_filters[decoder_id].end())
				{
					_link_decoder_to_filters[decoder_id].push_back(filter_id);
				}

				// Encoding: Filter(1) -> Encoder (1)
				_link_filter_to_encoder[filter_id] = encoder_id;

				// Flushing: Encoder(1) -> OutputTrack (N)
				_link_encoder_to_outputs[encoder_id].push_back(make_pair(output_stream, output_track_id));
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
		debug_log.AppendFormat(" \n");
		debug_log.AppendFormat("* InputTrack(%s/%d) : %s\n",
							   _input_stream->GetName().CStr(),
							   input_track_id,
							   input_track->GetInfoString().CStr());

		// Bypass Stream
		if (_link_input_to_outputs.find(input_track_id) != _link_input_to_outputs.end())
		{
			auto &output_tracks = _link_input_to_outputs[input_track_id];
			for (auto &[stream, track_id] : output_tracks)
			{
				auto output_track = stream->GetTrack(track_id);

				debug_log.AppendFormat("    + (Passthrough) OutputTrack(%s/%d) : %s\n",
									   stream->GetName().CStr(),
									   track_id,
									   output_track->GetInfoString().CStr());
			}
		}

		// Transcoding Stream
		if (_link_input_to_decoder.find(input_track_id) != _link_input_to_decoder.end())
		{
			auto decoder_id = _link_input_to_decoder[input_track_id];
			debug_log.AppendFormat("    + Decoder(%d)\n", decoder_id);

			if (_link_decoder_to_filters.find(decoder_id) != _link_decoder_to_filters.end())
			{
				auto &filter_ids = _link_decoder_to_filters[decoder_id];
				for (auto &filter_id : filter_ids)
				{
					debug_log.AppendFormat("        + Filter(%d)\n", filter_id);

					if (_link_filter_to_encoder.find(filter_id) != _link_filter_to_encoder.end())
					{
						auto encoder_id = _link_filter_to_encoder[filter_id];
						debug_log.AppendFormat("            + Encoder(%d)\n", encoder_id);

						if (_link_encoder_to_outputs.find(encoder_id) != _link_encoder_to_outputs.end())
						{
							auto &output_tracks = _link_encoder_to_outputs[encoder_id];
							for (auto &[stream, track_id] : output_tracks)
							{
								auto output_track = stream->GetTrack(track_id);

								debug_log.AppendFormat("                  + OutputTrack(%s/%d) : %s\n",
													   stream->GetName().CStr(),
													   track_id,
													   output_track->GetInfoString().CStr());
							}
						}
					}
				}
			}
		}
	}

	return debug_log;
}

void TranscoderStream::AddComposite(
	ov::String profile_sign,
	std::shared_ptr<info::Stream> input_stream,	std::shared_ptr<MediaTrack> input_track,
	std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	auto key = std::make_pair(profile_sign, input_track->GetMediaType());

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
	int32_t created_decoder_count = 0;

	for (auto &[input_track_id, decoder_id] : _link_input_to_decoder)
	{
		auto track_item = _input_stream->GetTracks().find(input_track_id);
		if (track_item == _input_stream->GetTracks().end())
		{
			continue;
		}

		auto &track = track_item->second;

		// Get hardware acceleration is enabled
		auto use_hwaccel = _application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration();
		track->SetHardwareAccel(use_hwaccel);

		// Deprecated
		// Set the number of b frames for compatibility with specific encoders.
		// Default is 16. refer to .../config/.../applications/decodes.h
		[[maybe_unused]] auto h264_has_bframes = _application_info.GetConfig().GetDecodes().GetH264hasBFrames();
		// transcode_context->SetH264hasBframes(h264_has_bframes);

		if (CreateDecoder(decoder_id, track) == false)
		{
			continue;
		}

		created_decoder_count++;
	}

	return created_decoder_count;
}


bool TranscoderStream::CreateDecoder(int32_t decoder_id, std::shared_ptr<MediaTrack> input_track)
{
	std::lock_guard<std::shared_mutex> decoder_lock(_decoder_map_mutex);

	logtd("[%s/%s(%u)] Create Decoder. InputTrack(%d) > Decoder(%d)", _input_stream->GetApplicationName(), _input_stream->GetName().CStr(), _input_stream->GetId(), input_track->GetId(), decoder_id);

	if(_decoders.find(decoder_id) != _decoders.end())
	{
		logte("[%s/%s(%u)] Decoder already exists. InputTrack(%d) > Decoder(%d)", _input_stream->GetApplicationName(), _input_stream->GetName().CStr(), _input_stream->GetId(), input_track->GetId(), decoder_id);
		return true;
	}

	auto decoder = TranscodeDecoder::Create(decoder_id, *_input_stream, input_track, bind(&TranscoderStream::OnDecodedFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	if (decoder == nullptr)
	{
		logte("[%s/%s(%u)] Decoder allocation failed", _input_stream->GetApplicationName(), _input_stream->GetName().CStr(), _input_stream->GetId());

		return false;
	}

	_decoders[decoder_id] = std::move(decoder);

	return true;
}

int32_t TranscoderStream::CreateEncoders(MediaFrame *buffer)
{
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

			logtd("[%s/%s(%u)] InputTrack(%d) > Encoder(%d) > StreamName(%s) > OutputTrack(%d)", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
				  track_id, encoder_id, output_stream->GetName().CStr(), output_track->GetId());

			auto use_hwaccel = _application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration();
			output_track->SetHardwareAccel(use_hwaccel);

			if (CreateEncoder(encoder_id, output_stream, output_track) == false)
			{
				logte("[%s/%s(%u)] Could not create encoder. Encoder(%d), OutputTrack(%d)", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), encoder_id, output_track->GetId());
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
		}
	}

	return 0;
}

bool TranscoderStream::CreateEncoder(int32_t encoder_id, std::shared_ptr<info::Stream> &output_stream, std::shared_ptr<MediaTrack> &output_track)
{
	std::lock_guard<std::shared_mutex> encoder_lock(_encoder_map_mutex);

	// If there is an existing encoder, do not create encoder
	if (_encoders.find(encoder_id) != _encoders.end())
	{
		logtd("[%s/%s(%u)] Encoder have already been created and will be shared. Encoder(%d), OutputTrack(%d)", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), encoder_id, output_track->GetId());
		return true;
	}

	auto encoder = TranscodeEncoder::Create(encoder_id, *output_stream, output_track, bind(&TranscoderStream::OnEncodedPacket, this, std::placeholders::_1, std::placeholders::_2));
	if (encoder == nullptr)
	{
		logte("%d track encoder allocation failed", encoder_id);
		return false;
	}

	_encoders[encoder_id] = std::move(encoder);

	return true;
}


int32_t TranscoderStream::CreateFilters(MediaFrame *buffer)
{
	int32_t created_count = 0;

	MediaTrackId decoder_id = buffer->GetTrackId();

	// 1. Get Input Track of Decoder
	auto input_track = _decoders[decoder_id]->GetRefTrack();

	auto decoder_to_filters_it = _link_decoder_to_filters.find(decoder_id);
	if (decoder_to_filters_it == _link_decoder_to_filters.end())
	{
		logtw("%s Could not found filter list related to decoder", _log_prefix.CStr());
		return created_count;
	}
	auto filter_ids = decoder_to_filters_it->second;

	// 2. Get Output Track of Encoders
	for (auto &filter_id : filter_ids)
	{
		MediaTrackId encoder_id = _link_filter_to_encoder[filter_id];
		if (_encoders.find(encoder_id) == _encoders.end())
		{
			logte("%s encoder is not allocated, Encoder(%d)", _log_prefix.CStr(), encoder_id);
			continue;
		}

		auto output_track = _encoders[encoder_id]->GetRefTrack();

		logtd("%s Create Filter. Decoder(%d) > Filter(%d) > Encoder(%d)", _log_prefix.CStr(), decoder_id, filter_id, encoder_id);
		if(CreateFilter(filter_id, input_track, output_track) == false)
		{
			continue;
		}

		created_count++;
	}

	return created_count;
}

bool TranscoderStream::CreateFilter(int32_t filter_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<MediaTrack> output_track)
{
	std::lock_guard<std::shared_mutex> filter_lock(_filter_map_mutex);

	// remove the previous created filter
	if (_filters.find(filter_id) != _filters.end())
	{
		_filters[filter_id]->Stop();
		_filters[filter_id] = nullptr;
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

	return true;
}


// Function called when codec information is extracted or changed from the decoder
void TranscoderStream::ChangeOutputFormat(MediaFrame *buffer)
{
	std::lock_guard<std::shared_mutex> lock(_format_change_mutex);

	if (buffer == nullptr)
	{
		logte("%s Invalid media buffer", _log_prefix.CStr());
		return;
	}

	logtd("%s Changed output format. InputTrack(%d)", _log_prefix.CStr(), buffer->GetTrackId());

	// Update Track of Input Stream
	UpdateInputTrack(buffer);

	// Update Track of Output Stream
	UpdateOutputTrack(buffer);

	// Create Encoder
	CreateEncoders(buffer);

	// Create Filter
	CreateFilters(buffer);
}

// Information of the input track is updated by the decoded frame
void TranscoderStream::UpdateInputTrack(MediaFrame *buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	logtd("%s Updated input track. InputTrack(%d)", _log_prefix.CStr(), track_id);

	auto &input_track = _input_stream->GetTrack(track_id);
	if (input_track == nullptr)
	{
		logte("Could not found output track. InputTrack(%d)", track_id);
		return;
	}

	switch (input_track->GetMediaType())
	{
		case cmn::MediaType::Video: {
			input_track->SetWidth(buffer->GetWidth());
			input_track->SetHeight(buffer->GetHeight());
			input_track->SetColorspace(buffer->GetFormat());  // used AVPixelFormat
		}
		break;
		case cmn::MediaType::Audio: {
			input_track->SetSampleRate(buffer->GetSampleRate());
			input_track->SetChannel(buffer->GetChannels());
			input_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(buffer->GetFormat()));
		}
		break;
		default: {
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
		(void)(key);

		if (input_track_id != composite->GetInputTrack()->GetId())
		{
			continue;
		}

		logtd("%s Updated output track. InputTrack(%d), Signature(%s)", _log_prefix.CStr(), input_track_id, key.first.CStr());

		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			UNUSED_VARIABLE(output_stream)

			// Case of ByPass
			if (output_track->IsBypass() == true)
			{
				if (output_track->GetMediaType() == cmn::MediaType::Video)
				{
					output_track->SetWidth(buffer->GetWidth());
					output_track->SetHeight(buffer->GetHeight());
					output_track->SetColorspace(buffer->GetFormat());
				}
				else if (output_track->GetMediaType() == cmn::MediaType::Audio)
				{
					output_track->SetSampleRate(buffer->GetSampleRate());
					output_track->GetSample().SetFormat(buffer->GetFormat<cmn::AudioSample::Format>());
					output_track->SetChannel(buffer->GetChannels());
				}
			}
			else
			{
				if (output_track->GetMediaType() == cmn::MediaType::Video)
				{

					// Keep the original video resolution
					if (output_track->GetWidth() == 0 && output_track->GetHeight() == 0)
					{
						output_track->SetWidth(buffer->GetWidth());
						output_track->SetHeight(buffer->GetHeight());
					}
					// Width is automatically calculated as the original video ratio					
					else if (output_track->GetWidth() == 0 && output_track->GetHeight() != 0)
					{
						float aspect_ratio = (float)buffer->GetWidth() / (float)buffer->GetHeight();
						int32_t width = (int32_t)((float)output_track->GetHeight() * aspect_ratio);
						width = (width % 2 == 0) ? width : width + 1;
						output_track->SetWidth(width);
					}
						// Heigh is automatically calculated as the original video ratio
					else if (output_track->GetWidth() != 0 && output_track->GetHeight() == 0)
					{
						float aspect_ratio = (float)buffer->GetWidth() / (float)buffer->GetHeight();
						int32_t height = (int32_t)((float)output_track->GetWidth() / aspect_ratio);
						height = (height % 2 == 0) ? height : height + 1;
						output_track->SetHeight(height);
					}

					// Set framerate of the output track
					if (output_track->GetFrameRate() == 0.0f)
					{
						auto &input_track = _input_stream->GetTrack(input_track_id);

						if (input_track->GetFrameRate() != 0.0f)
						{
							output_track->SetEstimateFrameRate(input_track->GetFrameRate());
							logti("Framerate of the output stream is not set. set the estimated framerate from framerate of input track. %.2ffps", output_track->GetEstimateFrameRate());
						}
						else if (input_track->GetEstimateFrameRate() != 0.0f)
						{
							output_track->SetEstimateFrameRate(input_track->GetEstimateFrameRate());
							logti("Framerate of the output stream is not set. set the estimated framerate from estimated framerate of input track. %.2ffps", output_track->GetEstimateFrameRate());
						}
						else
						{
							float estimated_framerate = 1 / ((double)buffer->GetDuration() * input_track->GetTimeBase().GetExpr());
							output_track->SetEstimateFrameRate(estimated_framerate);
							logti("Framerate of the output stream is not set. set the estimated framerate from decoded frame duration. %.2ffps, duration(%lld) tb.expr(%f)", output_track->GetEstimateFrameRate(), buffer->GetDuration(), input_track->GetTimeBase().GetExpr());
						}
					}
				}
				else if (output_track->GetMediaType() == cmn::MediaType::Audio)
				{
					if (output_track->GetSampleRate() == 0)
					{
						output_track->SetSampleRate(buffer->GetSampleRate());
						output_track->SetTimeBase(1, buffer->GetSampleRate());
					}

					if (output_track->GetChannel().GetLayout() == cmn::AudioChannel::Layout::LayoutUnknown)
					{
						output_track->SetChannel(buffer->GetChannels());
					}
				}
			}
		}
	}
}


void TranscoderStream::DecodePacket(std::shared_ptr<MediaPacket> packet)
{
	MediaTrackId input_track_id = packet->GetTrackId();

	// 1. bypass track processing.
	auto output_streams_it = _link_input_to_outputs.find(input_track_id);
	if (output_streams_it != _link_input_to_outputs.end())
	{
		auto &output_tracks = output_streams_it->second;

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
				logte("%s Could not found output track. InputTrack(%d)", _log_prefix.CStr(), output_track_id);
				continue;
			}

			auto clone_packet = packet->ClonePacket();

			clone_packet->SetTrackId(output_track_id);

			// PTS/DTS recalculation based on output timebase
			double scale = input_track->GetTimeBase().GetExpr() / output_track->GetTimeBase().GetExpr();
			clone_packet->SetPts((int64_t)((double)clone_packet->GetPts() * scale));
			clone_packet->SetDts((int64_t)((double)clone_packet->GetDts() * scale));

			SendFrame(output_stream, std::move(clone_packet));
		}
	}

	// 2. decoding track processing
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

void TranscoderStream::OnDecodedFrame(TranscodeResult result, int32_t decoder_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	if (decoded_frame == nullptr)
	{
		return;
	}

	switch (result)
	{
		case TranscodeResult::NoData: {
 #if GENERATE_FILLER_FRAME
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

			// Send Temporary Frame to Filter
			SpreadToFilters(decoder_id, last_frame);
#endif // End of Filler Frame Generation
		}
		break;

		// It indicates output format is changed
		case TranscodeResult::FormatChanged: {

			// Re-create filter and encoder using the format of decoded frame
			ChangeOutputFormat(decoded_frame.get());

 #if GENERATE_FILLER_FRAME
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

					logtd("Number of fillter frames needed. Decoder(%d), NeededFrames(%d)", decoder_id, number_of_filler_frames_needed);

					if(number_of_filler_frames_needed > 0)					
					{
						int64_t frame_hole_time_tb = (int64_t)((double)frame_hole_time_us / input_track->GetTimeBase().GetExpr() / 1000000);
						int64_t frame_duration_avg = frame_hole_time_tb / number_of_filler_frames_needed;
						int64_t start_pts = decoded_frame->GetPts() - frame_hole_time_tb + frame_duration_avg;
						int64_t end_pts = decoded_frame->GetPts();
						
						for(int64_t filler_pts = start_pts ; filler_pts < end_pts ; filler_pts+=frame_duration_avg)
						{
							// logtd("Spread to filter. [%d], filter_frame_pts(%lld), avg_frame_duration(%lld)", decoder_id, filler_pts, frame_duration_avg);

							// TODO(soulk): More tests are needed.
							// Video frames do not create filler frames. 
							// Even if there are no filler frames of the video type, there is no problem with AV sync in the player.
							if (input_track->GetMediaType() == cmn::MediaType::Audio)
							{
								auto cloned_filler_frame = decoded_frame->CloneFrame();
								cloned_filler_frame->SetPts(filler_pts);

								// Fill the silence in the audio frame.
								cloned_filler_frame->FillZeroData();
								SpreadToFilters(decoder_id, cloned_filler_frame);
							}
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

void TranscoderStream::SetLastDecodedFrame(int32_t decoder_id, std::shared_ptr<MediaFrame> &decoded_frame)
{
	_last_decoded_frames[decoder_id] = decoded_frame->CloneFrame();
}

std::shared_ptr<MediaFrame> TranscoderStream::GetLastDecodedFrame(int32_t decoder_id)
{
	if (_last_decoded_frames.find(decoder_id) != _last_decoded_frames.end())
	{
		auto frame = _last_decoded_frames[decoder_id]->CloneFrame();
		frame->SetTrackId(decoder_id);
		return frame;
	}

	return nullptr;
}

std::shared_ptr<MediaTrack> TranscoderStream::GetInputTrackOfFilter(int32_t decoder_id)
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

TranscodeResult TranscoderStream::FilterFrame(int32_t filter_id, std::shared_ptr<MediaFrame> decoded_frame)
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

	return TranscodeResult::NoData;
}

void TranscoderStream::OnFilteredFrame(int32_t filter_id, std::shared_ptr<MediaFrame> filtered_frame)
{
	filtered_frame->SetTrackId(filter_id);

	EncodeFrame(std::move(filtered_frame));
}

TranscodeResult TranscoderStream::EncodeFrame(std::shared_ptr<const MediaFrame> frame)
{
	auto filter_id = frame->GetTrackId();

	auto filter_to_encoder_it = _link_filter_to_encoder.find(filter_id);
	if(filter_to_encoder_it == _link_filter_to_encoder.end())
	{
		return TranscodeResult::NoData;
	}
	auto encoder_id = filter_to_encoder_it->second;

	std::shared_lock<std::shared_mutex> lock(_encoder_map_mutex);
	
	auto encoder_map_it = _encoders.find(encoder_id);
	if (encoder_map_it == _encoders.end())
	{
		return TranscodeResult::NoData;
	}
	auto encoder = encoder_map_it->second.get();

	encoder->SendBuffer(std::move(frame));

	return TranscodeResult::NoData;
}

// Callback is called from the encoder for packets that have been encoded.
void TranscoderStream::OnEncodedPacket(int32_t encoder_id, std::shared_ptr<MediaPacket> encoded_packet)
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
		auto clone_packet = encoded_packet->ClonePacket();
		clone_packet->SetTrackId(output_track_id);

		// Send the packet to MediaRouter
		SendFrame(output_stream, std::move(clone_packet));
	}
}

void TranscoderStream::NotifyCreateStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		bool ret = _parent->CreateStream(output_stream);
		if (ret == false)
		{
			logtw("%s Could not create stream. [%s/%s(%u)]", _log_prefix.CStr(), _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());
		}
	}
}

void TranscoderStream::NotifyDeleteStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		logti("%s Output stream has been deleted. %s/%s(%u)",
			  _log_prefix.CStr(),
			  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

		_parent->DeleteStream(output_stream);
	}

	_output_streams.clear();
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

void TranscoderStream::NotifyUpdateStreams()
{
	for (auto &[output_stream_name, output_stream] : _output_streams)
	{
		UNUSED_VARIABLE(output_stream_name)

		logti("%s Output stream has been updated. %s/%s(%u)",
			  _log_prefix.CStr(),
			  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

		_parent->UpdateStream(output_stream);
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

void TranscoderStream::SpreadToFilters(int32_t decoder_id, std::shared_ptr<MediaFrame> frame)
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
		auto frame_clone = frame->CloneFrame();
		if (frame_clone == nullptr)
		{
			logte("%s Failed to clone frame", _log_prefix.CStr());

			continue;
		}

		FilterFrame(filter_id, std::move(frame_clone));
	}
}
