//==============================================================================
//
//  TranscoderStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_stream.h"

#include <config/config_manager.h>

#include "transcoder_application.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 100

TranscoderStream::TranscoderStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _application_info(application_info)
{
	logtd("Trying to create transcode stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	//store Parent information
	_parent = parent;

	// Store Stream information
	_input_stream = stream;
}

TranscoderStream::~TranscoderStream()
{
	Stop();

	// Delete all encoders, filters, decodres
	_encoders.clear();
	_filters.clear();
	_decoders.clear();

	// Delete all map of stage
	_stage_input_to_decoder.clear();
	_stage_input_to_outputs.clear();
	_stage_decoder_to_filters.clear();
	_stage_filter_to_encoder.clear();
	_stage_encoder_to_outputs.clear();

	_output_streams.clear();

	_last_decoded_frames.clear();
}

info::stream_id_t TranscoderStream::GetStreamId()
{
	if (_input_stream != nullptr)
		return _input_stream->GetId();

	return 0;
}

bool TranscoderStream::Start()
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

	if (CreatePipeline() == 0)
	{
		logte("No stage generated");
		return false;
	}

	if (CreateDecoders() == 0)
	{
		logti("No decoder generated");
	}

	// Notify to create a new stream on the media router.
	NotifyCreateStreams();

	logti("[%s/%s(%u)] Transcoder stream has been started.",
		  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	return true;
}

bool TranscoderStream::Stop()
{
	logtd("Wait for terminated trancode stream thread.");

	RemoveAllComponents();

	// Notify to delete the stream created on the MediaRouter
	NotifyDeleteStreams();

	logti("[%s/%s(%u)] Transcoder stream has been stopped.", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	return true;
}

void TranscoderStream::RemoveAllComponents()
{
	// Stop all encoders
	RemoveEncoders();

	// Stop all filters
	RemoveFilters();

	// Stop all decoder
	RemoveDecoders();
}

void TranscoderStream::RemoveDecoders()
{
	for (auto &it : _decoders)
	{
		auto object = it.second;
		object->Stop();
		object.reset();
	}
}

void TranscoderStream::RemoveFilters()
{
	for (auto &it : _filters)
	{
		auto object = it.second;
		object->Stop();
		object.reset();
	}
}

void TranscoderStream::RemoveEncoders()
{
	for (auto &iter : _encoders)
	{
		auto object = iter.second;
		object->Stop();
		object.reset();
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

bool TranscoderStream::Prepare()
{
	logtd("[%s/%s(%u)] Prepare. msid(%u)", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), _input_stream->GetMsid());

	return true;
}

bool TranscoderStream::IsAvailableSmoothTransitionStream(const std::shared_ptr<info::Stream> &stream)
{
	int32_t track_count_per_mediatype[(uint32_t)cmn::MediaType::Nb] = {0};

	// Rule 1: The number of tracks per media type should not exceed one.
	for (const auto &[track_id, track] : stream->GetTracks())
	{
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

bool TranscoderStream::Update(const std::shared_ptr<info::Stream> &stream)
{
	logtd("[%s/%s(%u)] Update. msid(%u)", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), stream->GetMsid());

	// Check if smooth stream transition is possible
	// [Rule]
	// - The number of tracks per media type should not exceed one.
	// - The number of tracks of the stream to be changed must be the same.
	if (IsAvailableSmoothTransitionStream(stream) == true)
	{
		logti("[%s/%s(%u)] This stream will be a smooth transition", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

		RemoveDecoders();

		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());
	}
	else
	{
		logti("[%s/%s(%u)] This stream does not support smooth transitions. renew the encoder", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

		RemoveAllComponents();

		CreateDecoders();

		UpdateMsidOfOutputStreams(stream->GetMsid());

		NotifyUpdateStreams();
	}

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
	int32_t created_stream_count = 0;

	auto output_stream = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);
	if (output_stream == nullptr)
	{
		return created_stream_count;
	}

	output_stream->SetName(_input_stream->GetName());
	output_stream->SetMediaSource(_input_stream->GetUUID());
	output_stream->LinkInputStream(_input_stream);

	for (auto &it : _input_stream->GetTracks())
	{
		auto &input_track = it.second;
		auto output_track = std::make_shared<MediaTrack>();
		if (output_track == nullptr)
		{
			continue;
		}

		//TODO(Keukhan) : add a mediatrack copy(or clone) function.
		output_track->SetBypass(true);
		output_track->SetMediaType(input_track->GetMediaType());
		output_track->SetId(NewTrackId());
		output_track->SetBitrate(input_track->GetBitrate());
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetOriginBitstream(input_track->GetOriginBitstream());

		output_track->SetWidth(input_track->GetWidth());
		output_track->SetHeight(input_track->GetHeight());
		output_track->SetFrameRate(input_track->GetFrameRate());

		output_track->GetChannel().SetLayout(input_track->GetChannel().GetLayout());
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());

		output_track->SetTimeBase(GetDefaultTimebaseByCodecId(input_track->GetCodecId()));

		if (input_track->GetCodecId() == cmn::MediaCodecId::Opus && input_track->GetSampleRate() != 48000)
		{
			logtw("OPUS codec only supports 48000Hz samplerate. Do not create bypass track. input smplereate(%d)", input_track->GetSampleRate());
			continue;
		}

		output_stream->AddTrack(output_track);
		AddCompositeMap("dynamic", _input_stream, input_track, output_stream, output_track);
	}

	// Add to Output Stream List. The key is the output stream name.
	_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream));

	logti("[%s/%s(%u)] -> [%s/%s(%u)] dynamic output stream has been created",
		  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
		  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

	// Number of generated output streams
	created_stream_count++;

	return created_stream_count;
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

	// It helps modules to reconize origin stream from provider
	stream->LinkInputStream(_input_stream);
	stream->SetMediaSource(_input_stream->GetUUID());

	// Create a new stream name.
	auto name = cfg_output_profile.GetOutputStreamName();
	if (::strstr(name.CStr(), "${OriginStreamName}") != nullptr)
	{
		name = name.Replace("${OriginStreamName}", _input_stream->GetName());
	}
	stream->SetName(name);

	bool is_parsed = false;
	auto cfg_playlists = cfg_output_profile.GetPlaylists(&is_parsed);
	if (is_parsed)
	{
		for (const auto &cfg_playlist : cfg_playlists)
		{
			auto playlist = std::make_shared<info::Playlist>(cfg_playlist.GetName(), cfg_playlist.GetFileName());

			for (const auto &cfg_rendition : cfg_playlist.GetRenditions())
			{
				playlist->AdddRendition(std::make_shared<info::Rendition>(cfg_rendition.GetName(), cfg_rendition.GetVideoName(), cfg_rendition.GetAudioName()));
			}

			stream->AddPlaylist(playlist);
		}
	}

	return stream;
}

std::shared_ptr<MediaTrack> TranscoderStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId());
	output_track->SetName(profile.GetName());

	if (profile.IsBypass() == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetWidth(input_track->GetWidth());
		output_track->SetHeight(input_track->GetHeight());
		output_track->SetFrameRate(input_track->GetFrameRate());
		output_track->SetTimeBase(input_track->GetTimeBase());

		bool is_parsed;
		profile.GetBitrateString(&is_parsed);
		if (is_parsed == true)
		{
			// If bitrates information is not provided in the input stream or if it is ABR,
			// the user can input information manually and use it.
			// Bitrates information is essential in ABR.
			output_track->SetBitrate(profile.GetBitrate());
		}
		else
		{
			output_track->SetBitrate(input_track->GetBitrate());
		}
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetBitrate(profile.GetBitrate());
		output_track->SetWidth(profile.GetWidth());
		output_track->SetHeight(profile.GetHeight());
		output_track->SetFrameRate(profile.GetFramerate());
		output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
		output_track->SetPreset(profile.GetPreset());
		output_track->SetThreadCount(profile.GetThreadCount());
	}

	if (cmn::IsVideoCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscoderStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId());

	output_track->SetBypass(false);
	output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
	output_track->SetBitrate(0);
	output_track->SetWidth(profile.GetWidth());
	output_track->SetHeight(profile.GetHeight());
	output_track->SetFrameRate(profile.GetFramerate());
	output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));

	if (cmn::IsVideoCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscoderStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Audio);
	output_track->SetId(NewTrackId());
	output_track->SetName(profile.GetName());

	if (profile.IsBypass() == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetChannel(input_track->GetChannel());
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
		output_track->SetTimeBase(input_track->GetTimeBase());
		output_track->SetSampleRate(input_track->GetSampleRate());

		if (output_track->GetCodecId() == cmn::MediaCodecId::Opus && output_track->GetSampleRate() != 48000)
		{
			logtw("OPUS codec only supports 48000Hz samplerate. Do not create bypass track");
			return nullptr;
		}

		bool is_parsed;
		profile.GetBitrateString(&is_parsed);
		if (is_parsed == true)
		{
			// If bitrates information is not provided in the input stream or if it is ABR,
			// the user can input information manually and use it.
			// Bitrates information is essential in ABR.
			output_track->SetBitrate(profile.GetBitrate());
		}
		else
		{
			output_track->SetBitrate(input_track->GetBitrate());
		}
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetBitrate(profile.GetBitrate());
		output_track->GetChannel().SetLayout(profile.GetChannel() == 1 ? cmn::AudioChannel::Layout::LayoutMono : cmn::AudioChannel::Layout::LayoutStereo);
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());	// The sample format will change by the decoder event.
		output_track->SetSampleRate(profile.GetSamplerate());

		if (output_track->GetCodecId() == cmn::MediaCodecId::Opus)
		{
			if (output_track->GetSampleRate() != 48000)
			{
				output_track->SetSampleRate(48000);
				logtw("OPUS codec only supports 48000Hz samplerate. change the samplerate to 48000Hz");
			}
		}

		if (output_track->GetSampleRate() == 0)
		{
			output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
		}
		else
		{
			output_track->SetTimeBase(1, output_track->GetSampleRate());
		}
	}

	if (cmn::IsAudioCodec(output_track->GetCodecId()) == false)
	{
		logtw("Encoding codec set is not a audio codec");
		return nullptr;
	}

	return output_track;
}

// Create Output Stream and Track
int32_t TranscoderStream::CreateOutputStreams()
{
	int32_t created_stream_count = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create new stream and new track
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Get [application->Streams] list of application configuration.
	auto &cfg_output_profile_list = _application_info.GetConfig().GetOutputProfileList();

	// Get the encoding profile to make the output stream
	for (const auto &cfg_output_profile : cfg_output_profile_list)
	{
		auto output_stream = CreateOutputStream(cfg_output_profile);
		if (output_stream == nullptr)
		{
			logte("Cloud not create output stream");
			continue;
		}

		// Get all the tracks in the input stream to make output track
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
							logtw("Encoding codec set is not a video codec, track_id(%d)", input_track_id);
							continue;
						}

						output_stream->AddTrack(output_track);

						AddCompositeMap(GetIdentifiedForVideoProfile(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
					}

					// Image Profile
					for (auto &profile : cfg_output_profile.GetEncodes().GetImageProfileList())
					{
						auto output_track = CreateOutputTrack(input_track, profile);
						if (output_track == nullptr)
						{
							logtw("Encoding codec set is not a video codec, track_id(%d)", input_track_id);
							continue;
						}

						output_stream->AddTrack(output_track);

						AddCompositeMap(GetIdentifiedForImageProfile(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
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
							logtw("Encoding codec set is not a audio codec, track_id(%d)", input_track_id);
							continue;
						}

						output_stream->AddTrack(output_track);

						AddCompositeMap(GetIdentifiedForAudioProfile(input_track_id, profile), _input_stream, input_track, output_stream, output_track);
					}
				}
				break;
				default: {
					logtw("Unsupported media type of input track. type(%d)", input_track->GetMediaType());
					continue;
				}
			}
		}

		// Add to Output Stream List. The key is the output stream name.
		_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream));

		logti("[%s/%s(%u)] -> [%s/%s(%u)] Output stream has been created",
			  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
			  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

		// Number of generated output streams
		created_stream_count++;
	}

	return created_stream_count;
}

int32_t TranscoderStream::CreatePipeline()
{
	int32_t created_count = 0;

	ov::String temp_debug_msg = "\r\nTranscoding Pipeline. ";
	temp_debug_msg.AppendFormat(" [%s/%s(%d)]\n", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	for (auto &[key, composite] : _composite_map)
	{
		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			auto input_track_id = composite->GetInputTrack()->GetId();
			auto decoder_id = composite->GetInputTrack()->GetId();
			auto filter_id = composite->GetId();
			auto encoder_id = composite->GetId();
			auto output_track_id = output_track->GetId();

			// [Flow] Input Track -> Output Track
			if (output_track->IsBypass() == true)
			{
				// Input Track -> Output Track
				_stage_input_to_outputs[input_track_id].push_back(make_pair(output_stream, output_track_id));
			}
			// [Flow] Input Track -> Decoder -> Filter -> Encoder -> Output Track
			else
			{
				// Input Track -> Decoder (1:1)
				_stage_input_to_decoder[input_track_id] = decoder_id;

				// Decoder -> Filter (1:N)
				if (_stage_decoder_to_filters.find(decoder_id) == _stage_decoder_to_filters.end())
				{
					_stage_decoder_to_filters[decoder_id].push_back(filter_id);
				}
				else
				{
					auto filter_ids = _stage_decoder_to_filters[decoder_id];
					bool found = false;

					for (auto fileter_id_it : filter_ids)
					{
						if (fileter_id_it == filter_id)
						{
							found = true;
						}
					}

					if (found == false)
					{
						_stage_decoder_to_filters[decoder_id].push_back(filter_id);
					}
				}

				// Filter -> Encoder (1:1)
				_stage_filter_to_encoder[filter_id] = encoder_id;

				// Encoder -> Output Track (1:N)
				_stage_encoder_to_outputs[encoder_id].push_back(make_pair(output_stream, output_track_id));
			}
		}

		// ---------------------------------------------
		// Debugging Log
		// ---------------------------------------------
		auto encode_profile_name = key.first;
		auto encode_media_type = key.second;

		ov::String temp_str = "";
		for (auto &[stream, track_info] : composite->GetOutputTracks())
		{
			temp_str.AppendFormat("%s(%u) [%d:%s]", stream->GetName().CStr(), stream->GetId(), track_info->GetId(), (track_info->IsBypass()) ? "Bypass" : "Encode");
		}

		temp_debug_msg.AppendFormat(" - Stage (%s/%32s) : InputTrack[%d] -> Decoder[%d] -> Filter[%d] -> Encoder[%d] -> OutputTrack[%s]\n",
									(encode_media_type == cmn::MediaType::Video) ? "Video" : "Audio",
									encode_profile_name.CStr(),
									composite->GetInputTrack()->GetId(),
									composite->GetInputTrack()->GetId(),
									composite->GetId(),
									composite->GetId(),
									temp_str.CStr());

		created_count++;
	}

	logtd(temp_debug_msg);

	return created_count;
}

ov::String TranscoderStream::GetIdentifiedForVideoProfile(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("T%d_PBYPASS", track_id);
	}

	return ov::String::FormatString("T%d_P%s-%d-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStream::GetIdentifiedForImageProfile(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	return ov::String::FormatString("T%d_P%s-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStream::GetIdentifiedForAudioProfile(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("T%d_PBYPASS");
	}

	return ov::String::FormatString("T%d_P%s-%d-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetSamplerate(),
									profile.GetChannel());
}

// Store information that is actually used during encoder profiles.
// This information is used to prevent encoder duplicate generation and map track IDs by stage.
void TranscoderStream::AddCompositeMap(
	ov::String unique_id,
	std::shared_ptr<info::Stream> input_stream,
	std::shared_ptr<MediaTrack> input_track,
	std::shared_ptr<info::Stream> output_stream,
	std::shared_ptr<MediaTrack> output_track)
{
	auto key = std::make_pair(unique_id, input_track->GetMediaType());

	auto it = _composite_map.find(key);
	if (it != _composite_map.end())
	{
		auto obj = it->second;
		obj->InsertOutput(output_stream, output_track);
		return;
	}

	auto obj = std::make_shared<CompositeContext>(_last_map_id++);
	obj->SetInput(input_stream, input_track);
	obj->InsertOutput(output_stream, output_track);

	_composite_map[key] = obj;
}

// Create Decoders
///
//  - Create docder by the number of tracks
// _decoders[track_id] + (transcode_context)

int32_t TranscoderStream::CreateDecoders()
{
	int32_t created_decoder_count = 0;

	for (auto iter : _stage_input_to_decoder)
	{
		auto input_track_id = iter.first;
		auto decoder_id = iter.second;

		auto track_item = _input_stream->GetTracks().find(input_track_id);
		if (track_item == _input_stream->GetTracks().end())
		{
			continue;
		}

		auto &track = track_item->second;

		std::shared_ptr<TranscodeContext> transcode_context = nullptr;

		switch (track->GetMediaType())
		{
			case cmn::MediaType::Video: {
				transcode_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetWidth(),
					track->GetHeight(),
					track->GetFrameRate(),
					track->GetFormat());
			}
			break;

			case cmn::MediaType::Audio: {
				transcode_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetSampleRate());
			}
			break;

			default:
				logtw("Not supported media type: %d", track->GetMediaType());
				continue;
		}

		transcode_context->SetTimeBase(track->GetTimeBase());

		// Get hardware acceleration is enabled
		auto use_hwaccel = _application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration();
		transcode_context->SetHardwareAccel(use_hwaccel);

		// Set the number of b frames for compatibility with specific encoders.
		// Default is 16. refer to .../config/.../applications/decodes.h
		auto h264_has_bframes = _application_info.GetConfig().GetDecodes().GetH264hasBFrames();
		transcode_context->SetH264hasBframes(h264_has_bframes);

		CreateDecoder(input_track_id, decoder_id, transcode_context);

		created_decoder_count++;
	}

	return created_decoder_count;
}

bool TranscoderStream::CreateDecoder(int32_t input_track_id, int32_t decoder_id, std::shared_ptr<TranscodeContext> input_context)
{
	logtd("[%s/%s(%u)] Create Decoder. inputTrackId: %d -> decoderId: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), input_track_id, decoder_id);

	// input_context must be decoding context
	OV_ASSERT2(input_context != nullptr);
	OV_ASSERT2(input_context->IsEncodingContext() == false);

	auto track = _input_stream->GetTrack(input_track_id);
	if (track == nullptr)
	{
		logte("Could not found media track");

		return false;
	}

	// create decoder for codec id
	auto decoder = TranscodeDecoder::Create(decoder_id, *_input_stream, input_context, bind(&TranscoderStream::OnDecodedFrame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	if (decoder == nullptr)
	{
		logte("Decoder allocation failed");

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
		if ((uint32_t)track_id != composite->GetInputTrack()->GetId())
		{
			continue;
		}

		auto encoder_id = composite->GetId();
		auto output_tracks_it = _stage_encoder_to_outputs.find(encoder_id);
		if (output_tracks_it == _stage_encoder_to_outputs.end())
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

			logtd("[%s/%s(%u)]  inputTrackId: %d -> encoderId:%d, streamName:%s, outputTrackId:%d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
				  track_id, encoder_id, output_stream->GetName().CStr(), output_track->GetId());

			switch (output_track->GetMediaType())
			{
				case cmn::MediaType::Video: {
					auto encoder_context = std::make_shared<TranscodeContext>(
						true,
						output_track->GetCodecId(),
						output_track->GetBitrate(),
						output_track->GetWidth(),
						output_track->GetHeight(),
						output_track->GetFrameRate(),
						output_track->GetFormat());

					// Get hardware acceleration is enabled
					auto use_hwaccel = _application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration();
					encoder_context->SetHardwareAccel(use_hwaccel);

					encoder_context->SetPreset(output_track->GetPreset());

					// If there is no framerate value, a constant bit rate is not produced by the encoder.
					// So, Estimated framerate is used to set the encoder.
					encoder_context->SetEstimateFrameRate(output_track->GetEsimateFrameRate());
					encoder_context->SetThreadCount(output_track->GetThreadCount());

					if (CreateEncoder(encoder_id, encoder_context) == false)
					{
						logte("Could not create encoder");
						continue;
					}

					// Get the color space supported by the generated encoder and sets it to Output Transcode Context
					encoder_context->SetColorspace(_encoders[encoder_id]->GetPixelFormat());
				}
				break;
				case cmn::MediaType::Audio: {
					auto encoder_context = std::make_shared<TranscodeContext>(
						true,
						output_track->GetCodecId(),
						output_track->GetBitrate(),
						output_track->GetSampleRate());

					// Get hardware acceleration is enabled
					auto use_hwaccel = _application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration();
					encoder_context->SetHardwareAccel(use_hwaccel);

					if (CreateEncoder(encoder_id, encoder_context) == false)
					{
						logte("Could not create encoder");
						continue;
					}
				}
				break;
				default: {
					logte("Unsupported media type. map_key:%s/%d", key.first, key.second);
				}
				break;
			}
		}
	}

	return 0;
}

bool TranscoderStream::CreateEncoder(int32_t encoder_id, std::shared_ptr<TranscodeContext> encoder_context)
{
	// If there is an existing encoder, do not create encoder
	if (_encoders.find(encoder_id) != _encoders.end())
	{
		return true;
	}

	auto encoder = TranscodeEncoder::Create(encoder_id, encoder_context, bind(&TranscoderStream::OnEncodedPacket, this, std::placeholders::_1, std::placeholders::_2));
	if (encoder == nullptr)
	{
		logte("%d track encoder allocation failed", encoder_id);
		return false;
	}

	_encoders[encoder_id] = std::move(encoder);

	return true;
}

void TranscoderStream::UpdateDecoderContext(MediaFrame *buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	logtd("[%s/%s(%u)] Update context of decoder. track_id: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), track_id);

	auto decoder = _decoders.find(track_id);
	if (decoder == _decoders.end())
	{
		return;
	}

	auto track = _input_stream->GetTrack(track_id);
	if (track == nullptr)
	{
		return;
	}

	// TranscodeContext of Decoder
	auto decoder_context = decoder->second->GetContext();

	switch (track->GetMediaType())
	{
		case cmn::MediaType::Video:
			decoder_context->SetBitrate(track->GetBitrate());
			decoder_context->SetVideoWidth(track->GetWidth());
			decoder_context->SetVideoHeight(track->GetHeight());
			decoder_context->SetFrameRate(track->GetFrameRate());
			decoder_context->SetTimeBase(track->GetTimeBase());
			break;

		case cmn::MediaType::Audio:
			decoder_context->SetBitrate(track->GetBitrate());
			decoder_context->SetAudioSampleRate(track->GetSampleRate());
			decoder_context->SetAudioChannel(track->GetChannel());
			decoder_context->SetAudioSampleFormat(track->GetSample().GetFormat());
			decoder_context->SetTimeBase(track->GetTimeBase());

			break;
		default:
			// Do nothing
			break;
	}
}

// Information of the input track is updated by the decoded frame
void TranscoderStream::UpdateInputTrack(MediaFrame *buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	auto &input_track = _input_stream->GetTrack(track_id);
	if (input_track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", track_id);
		return;
	}

	if (input_track->GetMediaType() == cmn::MediaType::Video)
	{
		input_track->SetWidth(buffer->GetWidth());
		input_track->SetHeight(buffer->GetHeight());
		input_track->SetFormat(buffer->GetFormat());
	}
	else if (input_track->GetMediaType() == cmn::MediaType::Audio)
	{
		input_track->SetSampleRate(buffer->GetSampleRate());
		input_track->GetSample().SetFormat(buffer->GetFormat<cmn::AudioSample::Format>());
		input_track->SetChannel(buffer->GetChannels());
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

		logtd("[%s/%s(%u)] UpdateOutputTrack. inputTrackId: %d, profile: %s ", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), input_track_id, key.first.CStr());

		for (auto &[output_stream, output_track] : composite->GetOutputTracks())
		{
			// Case of ByPass
			if (output_track->IsBypass() == true)
			{
				output_track->SetWidth(buffer->GetWidth());
				output_track->SetHeight(buffer->GetHeight());
				output_track->SetFormat(buffer->GetFormat());
				output_track->SetSampleRate(buffer->GetSampleRate());
				output_track->GetSample().SetFormat(buffer->GetFormat<cmn::AudioSample::Format>());
				output_track->SetChannel(buffer->GetChannels());
			}
			else
			{
				if (output_track->GetMediaType() == cmn::MediaType::Video)
				{
					if (output_track->GetWidth() == 0 && output_track->GetHeight() == 0)
					{
						// Keep the original video resolution
						output_track->SetWidth(buffer->GetWidth());
						output_track->SetHeight(buffer->GetHeight());
					}
					else if (output_track->GetWidth() == 0 && output_track->GetHeight() != 0)
					{
						// Width is automatically calculated as the original video ratio
						float aspect_raio = (float)buffer->GetWidth() / (float)buffer->GetHeight();
						int32_t width = (int32_t)((float)output_track->GetHeight() * aspect_raio);
						width = (width % 2 == 0) ? width : width + 1;
						output_track->SetWidth(width);
					}
					else if (output_track->GetWidth() != 0 && output_track->GetHeight() == 0)
					{
						// Heigh is automatically calculated as the original video ratio
						float aspect_raio = (float)buffer->GetWidth() / (float)buffer->GetHeight();
						int32_t height = (int32_t)((float)output_track->GetWidth() / aspect_raio);
						height = (height % 2 == 0) ? height : height + 1;
						output_track->SetHeight(height);
					}

					if (output_track->GetFrameRate() == 0.0f)
					{
						auto &input_track = _input_stream->GetTrack(input_track_id);
						float estimated_framerate = 1 / ((double)buffer->GetDuration() * input_track->GetTimeBase().GetExpr());
						logti("Framerate of the output stream is not set. set the estimated framerate of source stream. %.2ffps", estimated_framerate);
						output_track->SetEstimateFrameRate(estimated_framerate);
					}
				}

				if (output_track->GetMediaType() == cmn::MediaType::Audio)
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

				if (output_track->GetFormat() == 0)
				{
					output_track->SetFormat(buffer->GetFormat());
				}
			}
		}
	}
}

// Function called when codec information is extracted or changed from the decoder
void TranscoderStream::ChangeOutputFormat(MediaFrame *buffer)
{
	if (buffer == nullptr)
	{
		logte("Invalid media buffer");
		return;
	}

	logtd("[%s/%s(%u)] ChangeOutputFormat. track_id: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), buffer->GetTrackId());

	// Update Track of Input Stream
	UpdateInputTrack(buffer);

	// Update Decoder Context
	UpdateDecoderContext(buffer);

	// Udpate Track of Output Stream
	UpdateOutputTrack(buffer);

	// Create Encoder
	CreateEncoders(buffer);

	// Craete Filter
	CreateFilters(buffer);
}

void TranscoderStream::DecodePacket(std::shared_ptr<MediaPacket> packet)
{
	MediaTrackId input_track_id = packet->GetTrackId();

	// 1. bypass track processing
	auto output_streams_it = _stage_input_to_outputs.find(input_track_id);
	if (output_streams_it != _stage_input_to_outputs.end())
	{
		auto &output_tracks = output_streams_it->second;

		for (auto iter : output_tracks)
		{
			auto output_stream = iter.first;
			auto output_track_id = iter.second;

			auto input_track = _input_stream->GetTrack(input_track_id);
			if (input_track == nullptr)
			{
				logte("Could not found input track. track_id(%d)", input_track_id);
				continue;
			}

			auto output_track = output_stream->GetTrack(output_track_id);
			if (output_track == nullptr)
			{
				logte("Could not found output track. track_id(%d)", output_track_id);
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
	auto stage_item_decoder = _stage_input_to_decoder.find(input_track_id);
	if (stage_item_decoder == _stage_input_to_decoder.end())
	{
		return;
	}
	auto decoder_id = stage_item_decoder->second;

	auto decoder_item = _decoders.find(decoder_id);
	if (decoder_item == _decoders.end())
	{
		return;
	}
	auto decoder = decoder_item->second;
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
		// Distribute the last decoded frame to the Filter
		case TranscodeResult::NoData: {
			auto last_frame = GetLastDecodedFrame(decoder_id);
			if (last_frame == nullptr)
			{
				break;
			}

			auto input_track = GetInputTrack(decoder_id);
			auto filter_context = GetFilterInputContext(decoder_id);
			if (input_track == nullptr || filter_context == nullptr)
			{
				break;
			}

			// The decoded frame pts should be modified to fit the Timebase of the filter input.
			double input_expr = input_track->GetTimeBase().GetExpr();
			double filter_expr = filter_context->GetTimeBase().GetExpr();
			last_frame->SetPts((int64_t)((double)decoded_frame->GetPts() * input_expr / filter_expr));

			SpreadToFilters(decoder_id, last_frame);
		}
		break;

		// It indicates output format is changed
		case TranscodeResult::FormatChanged: {
			// Re-create filter and encoder using the format
			ChangeOutputFormat(decoded_frame.get());

			[[fallthrough]];
		}
		// The last decoded frame is temporarily stored and distributed to the filter.
		case TranscodeResult::DataReady: {
			SetLastDecodedFrame(decoder_id, decoded_frame);

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

std::shared_ptr<TranscodeContext> TranscoderStream::GetFilterInputContext(int32_t decoder_id)
{
	auto it = _stage_decoder_to_filters.find(decoder_id);
	if (it == _stage_decoder_to_filters.end())
	{
		return nullptr;
	}

	auto filter_ids = it->second;
	if (filter_ids.size() == 0)
	{
		return nullptr;
	}

	return _filters[filter_ids[0]]->_input_context;
}

TranscodeResult TranscoderStream::FilterFrame(int32_t filter_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	auto filter_item = _filters.find(filter_id);
	if (filter_item == _filters.end())
	{
		return TranscodeResult::NoData;
	}

	auto filter = filter_item->second.get();

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

	auto encoder_id = _stage_filter_to_encoder[filter_id];

	auto encoder_item = _encoders.find(encoder_id);
	if (encoder_item == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_item->second.get();

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
	auto stage_item = _stage_encoder_to_outputs.find(encoder_id);
	if (stage_item == _stage_encoder_to_outputs.end())
	{
		return;
	}

	// If a track exists to output, copy the encoded packet and send it to that track.
	for (auto &iter : stage_item->second)
	{
		auto &output_stream = iter.first;
		auto output_track_id = iter.second;

		auto clone_packet = encoded_packet->ClonePacket();
		clone_packet->SetTrackId(output_track_id);

		// Send the packet to MediaRouter
		SendFrame(output_stream, std::move(clone_packet));
	}
}

void TranscoderStream::NotifyCreateStreams()
{
	for (auto &iter : _output_streams)
	{
		auto stream_output = iter.second;

		bool ret = _parent->CreateStream(stream_output);
		if (ret == false)
		{
			logtw("Could not create stream. [%s/%s(%u)]", _application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());
		}
	}
}

void TranscoderStream::NotifyDeleteStreams()
{
	for (auto &iter : _output_streams)
	{
		auto stream_output = iter.second;
		logti("[%s/%s(%u)] -> [%s/%s(%u)] Transcoder output stream has been deleted.",
			  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
			  _application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());

		_parent->DeleteStream(stream_output);
	}

	_output_streams.clear();
}

void TranscoderStream::UpdateMsidOfOutputStreams(uint32_t msid)
{
	// Update info::Stream().msid of all output streams
	for (auto &iter : _output_streams)
	{
		auto stream_output = iter.second;

		stream_output->SetMsid(msid);
	}
}

void TranscoderStream::NotifyUpdateStreams()
{
	for (auto &iter : _output_streams)
	{
		auto stream_output = iter.second;
		logti("[%s/%s(%u)] -> [%s/%s(%u)] Transcoder output stream has been updated.",
			  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
			  _application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());

		_parent->UpdateStream(stream_output);
	}
}

void TranscoderStream::SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet)
{
	packet->SetMsid(stream->GetMsid());

	bool ret = _parent->SendFrame(stream, std::move(packet));
	if (ret == false)
	{
		logtw("[%s/%s(%u)] Could not send frame", _application_info.GetName().CStr(), stream->GetName().CStr(), stream->GetId());
	}
}

void TranscoderStream::CreateFilters(MediaFrame *buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	// 1. Input Context of Decoder
	auto &input_track = _input_stream->GetTrack(track_id);
	auto input_context = _decoders[track_id]->GetContext();

	auto filters = _stage_decoder_to_filters.find(track_id);
	if (filters == _stage_decoder_to_filters.end())
	{
		logtw("[%s/%s(%u)] Could not found filter", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());
		return;
	}

	auto filter_id_list = filters->second;
	for (auto &filter_id : filter_id_list)
	{
		// Remove an existing filter
		if (_filters.find(filter_id) != _filters.end())
		{
			_filters[filter_id]->Stop();
			_filters[filter_id] = nullptr;
		}

		auto encoder_id = _stage_filter_to_encoder[filter_id];
		if (_encoders.find(encoder_id) == _encoders.end())
		{
			logte("[%s/%s(%u)] encoder is not allocated, encoderId: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), encoder_id);
			continue;
		}

		logtd("[%s/%s(%u)] Create Filter. decoderId: %d -> filterId: %d -> encoderId: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), track_id, filter_id, encoder_id);

		// 2. Output Content of Encoder
		auto output_context = _encoders[encoder_id]->GetContext();

		auto filter = std::make_shared<TranscodeFilter>();
		filter->SetAlias(ov::String::FormatString("%s/%s", _application_info.GetName().CStr(), _input_stream->GetName().CStr()));
		bool ret = filter->Configure(filter_id, input_track, input_context, output_context, bind(&TranscoderStream::OnFilteredFrame, this, std::placeholders::_1, std::placeholders::_2));
		if (ret != true)
		{
			logte("[%s/%s(%u)] Failed to create filter. filterId: %d", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), filter_id);
			continue;
		}

		_filters[filter_id] = filter;
	}
}

void TranscoderStream::SpreadToFilters(int32_t decoder_id, std::shared_ptr<MediaFrame> frame)
{
	auto filters = _stage_decoder_to_filters.find(decoder_id);
	if (filters == _stage_decoder_to_filters.end())
	{
		logtw("Could not found filter");

		return;
	}

	for (auto &filter_id : filters->second)
	{
		auto frame_clone = frame->CloneFrame();
		if (frame_clone == nullptr)
		{
			logte("Failed to clone frame");

			continue;
		}

		FilterFrame(filter_id, std::move(frame_clone));
	}
}

MediaTrackId TranscoderStream::NewTrackId()
{
	return _last_track_index++;
}

const cmn::Timebase TranscoderStream::GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id)
{
	cmn::Timebase timebase(1, 1000);

	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::H265:
		case cmn::MediaCodecId::Vp8:
		case cmn::MediaCodecId::Vp9:
		case cmn::MediaCodecId::Flv:
		case cmn::MediaCodecId::Jpeg:
		case cmn::MediaCodecId::Png:
			timebase.SetNum(1);
			timebase.SetDen(90000);
			break;
		case cmn::MediaCodecId::Aac:
		case cmn::MediaCodecId::Mp3:
		case cmn::MediaCodecId::Opus:
			timebase.SetNum(1);
			timebase.SetDen(48000);
			break;
		default:
			break;
	}

	return timebase;
}
