//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_stream.h"

#include <config/config_manager.h>

#include "transcode_application.h"
#include "transcode_private.h"

#define MAX_QUEUE_SIZE 100

TranscodeStream::TranscodeStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _application_info(application_info)
{
	logtd("Trying to create transcode stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	//store Parent information
	_parent = parent;

	// Store Stream information
	_input_stream = stream;

	// for generating track ids
	_last_map_id = 0;
}

TranscodeStream::~TranscodeStream()
{
	// The thread checked for non-termination and terminated
	if (_kill_flag != true)
	{
		Stop();
	}

	// Delete all encoders, filters, decodres
	_encoders.clear();
	_filters.clear();
	_decoders.clear();

	// Delete all map of stage
	_stage_input_to_decoder.clear();
	_stage_input_to_output.clear();
	_stage_decoder_to_filter.clear();
	_stage_filter_to_encoder.clear();
	_stage_encoder_to_output.clear();

	_output_streams.clear();
}

TranscodeApplication *TranscodeStream::GetParent()
{
	return _parent;
}

info::stream_id_t TranscodeStream::GetStreamId()
{
	if (_input_stream != nullptr)
		return _input_stream->GetId();

	return 0;
}

bool TranscodeStream::Start()
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

	if (CreateStageMapping() == 0)
	{
		logte("No stage generated");
		return false;
	}

	if (CreateDecoders() == 0)
	{
		logti("No decoder generated");
	}

	_kill_flag = false;

	// Notify to create a new stream on the media router.
	NotifyCreateStreams();

	logti("[%s/%s(%u)] Transcoder input stream has been started. Status : (%d) Decoders, (%d) Encoders",
		  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(), _decoders.size(), _encoders.size());

	return true;
}

bool TranscodeStream::Stop()
{
	_kill_flag = true;

	logtd("Wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag ? "true" : "false");

	// Stop all encoders
	for (auto &iter : _encoders)
	{
		auto object = iter.second;
		object->Stop();
		object.reset();
	}

	// Stop all filters
	for (auto &it : _filters)
	{
		auto object = it.second;

		object.reset();
	}

	// Stop all decoder
	for (auto &it : _decoders)
	{
		auto object = it.second;
		object->Stop();
		object.reset();
	}

	// Notify to delete the stream created on the MediaRouter
	NotifyDeleteStreams();

	logti("[%s/%s(%u)] Transcoder stream has been stopped.", _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId());

	return true;
}

bool TranscodeStream::Push(std::shared_ptr<MediaPacket> packet)
{
	int32_t track_id = packet->GetTrackId();

	DecodePacket(track_id, std::move(packet));

	return true;
}

const cmn::Timebase TranscodeStream::GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id)
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

// Dynamically generated applications are created by default with BYPASS profiles.
int32_t TranscodeStream::CreateOutputStreamDynamic()
{
	int32_t created_stream_count = 0;

	auto output_stream = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);
	if (output_stream == nullptr)
	{
		return created_stream_count;
	}

	output_stream->SetName(_input_stream->GetName());
	output_stream->SetMediaSource(_input_stream->GetMediaSource());
	output_stream->SetOriginStream(_input_stream);

	for (auto &it : _input_stream->GetTracks())
	{
		auto &input_track = it.second;
		auto output_track = std::make_shared<MediaTrack>();
		if (output_track == nullptr)
		{
			continue;
		}

		//TODO(soulk) : add a mediatrack copy(or clone) function.
		output_track->SetBypass(true);
		output_track->SetMediaType(input_track->GetMediaType());
		output_track->SetId(NewTrackId(output_track->GetMediaType()));
		output_track->SetBitrate(input_track->GetBitrate());
		output_track->SetCodecId(input_track->GetCodecId());

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
		AppendTrackMap("dynamic", _input_stream, input_track, output_stream, output_track);
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

bool TranscodeStream::IsSupportedMediaType(const std::shared_ptr<MediaTrack> &media_track)
{
	auto track_media_type = media_track->GetMediaType();

	if (track_media_type != cmn::MediaType::Video && track_media_type != cmn::MediaType::Audio)
	{
		return false;
	}

	return true;
}

std::shared_ptr<info::Stream> TranscodeStream::CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile)
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
	stream->SetOriginStream(_input_stream);
	stream->SetMediaSource(_input_stream->GetMediaSource());

	// Create a new stream name.
	auto name = cfg_output_profile.GetOutputStreamName();
	if (::strstr(name.CStr(), "${OriginStreamName}") != nullptr)
	{
		name = name.Replace("${OriginStreamName}", _input_stream->GetName());
	}
	stream->SetName(name);

	return stream;
}

std::shared_ptr<MediaTrack> TranscodeStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId(output_track->GetMediaType()));

	if (profile.IsBypass() == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetBitrate(input_track->GetBitrate());
		output_track->SetWidth(input_track->GetWidth());
		output_track->SetHeight(input_track->GetHeight());
		output_track->SetFrameRate(input_track->GetFrameRate());
		output_track->SetTimeBase(input_track->GetTimeBase());
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(GetCodecId(profile.GetCodec()));
		output_track->SetBitrate(profile.GetBitrate());
		output_track->SetWidth(profile.GetWidth());
		output_track->SetHeight(profile.GetHeight());
		output_track->SetFrameRate(profile.GetFramerate());
		output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
	}

	if (IsVideoCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscodeStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId(output_track->GetMediaType()));

	output_track->SetBypass(false);
	output_track->SetCodecId(GetCodecId(profile.GetCodec()));
	output_track->SetBitrate(0);
	output_track->SetWidth((profile.GetWidth() == 0) ? input_track->GetWidth() : profile.GetWidth());
	output_track->SetHeight((profile.GetHeight() == 0) ? input_track->GetHeight() : profile.GetHeight());
	output_track->SetFrameRate((profile.GetFramerate() == 0) ? input_track->GetFrameRate() : profile.GetFramerate());
	output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));

	if (IsVideoCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscodeStream::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
		return nullptr;

	output_track->SetMediaType(cmn::MediaType::Audio);
	output_track->SetId(NewTrackId(output_track->GetMediaType()));

	if (profile.IsBypass() == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetBitrate(input_track->GetBitrate());
		output_track->GetChannel().SetLayout(input_track->GetChannel().GetLayout());
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
		output_track->SetTimeBase(input_track->GetTimeBase());
		output_track->SetSampleRate(input_track->GetTimeBase().GetDen());

		if (output_track->GetCodecId() == cmn::MediaCodecId::Opus && output_track->GetSampleRate() != 48000)
		{
			logtw("OPUS codec only supports 48000Hz samplerate. Do not create bypass track");
			return nullptr;
		}
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(GetCodecId(profile.GetCodec()));
		output_track->SetBitrate(profile.GetBitrate());
		output_track->GetChannel().SetLayout(profile.GetChannel() == 1 ? cmn::AudioChannel::Layout::LayoutMono : cmn::AudioChannel::Layout::LayoutStereo);
		// The sample format will change by the decoder event.
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
		output_track->SetSampleRate(profile.GetSamplerate());

		if (output_track->GetCodecId() == cmn::MediaCodecId::Opus)
		{
			if (output_track->GetSampleRate() != 48000)
			{
				output_track->SetSampleRate(48000);
				logtw("OPUS codec only supports 48000Hz samplerate. change the samplerate to 48000Hz");
			}
		}
		output_track->SetTimeBase(1, output_track->GetSampleRate());
	}

	if (IsAudioCodec(output_track->GetCodecId()) == false)
	{
		logtw("Encoding codec set is not a audio codec");
		return nullptr;
	}

	return output_track;
}

// Create Output Stream and Track
int32_t TranscodeStream::CreateOutputStreams()
{
	int32_t created_stream_count = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create new stream and new track
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Get [application->Streams] list of application configuration.
	auto &cfg_output_profile_list = _application_info.GetConfig().GetOutputProfileList();

	for (const auto &cfg_output_profile : cfg_output_profile_list)
	{
		auto output_stream = CreateOutputStream(cfg_output_profile);
		if (output_stream == nullptr)
		{
			logte("Cloud not create output stream");
			continue;
		}

		// Look up all tracks in the input stream.
		for (auto &[input_track_id, input_track] : _input_stream->GetTracks())
		{
			if (input_track->GetMediaType() == cmn::MediaType::Video)
			{
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

					AppendTrackMap(GetIdentifiedForVideoProfile(profile), _input_stream, input_track, output_stream, output_track);
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

					AppendTrackMap(GetIdentifiedForImageProfile(profile), _input_stream, input_track, output_stream, output_track);
				}
			}
			else if (input_track->GetMediaType() == cmn::MediaType::Audio)
			{
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
					AppendTrackMap(GetIdentifiedForAudioProfile(profile), _input_stream, input_track, output_stream, output_track);
				}
			}
			else
			{
				logtw("Unsupported media type of input track. type(%d)", input_track->GetMediaType());

				continue;
			}
		}

		// Add to Output Stream List. The key is the output stream name.
		_output_streams.insert(std::make_pair(output_stream->GetName(), output_stream));

		logti("[%s/%s(%u)] -> [%s/%s(%u)] Output stream has been created.",
			  _application_info.GetName().CStr(), _input_stream->GetName().CStr(), _input_stream->GetId(),
			  _application_info.GetName().CStr(), output_stream->GetName().CStr(), output_stream->GetId());

		// Number of generated output streams
		created_stream_count++;
	}

	return created_stream_count;
}

int32_t TranscodeStream::CreateStageMapping()
{
	int32_t created_stage_map = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. Configure Track Mapping by Stage
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Flow 1(Transcode) : InputTrack -> Decoder -> Filter -> Encoder -> Output Tracks
	// InputTrack = ID of Input Track
	// Decoder = ID of Input Track
	// Filter = ID of Generated by TranscodeTrackMapContext class
	// Encoder = ID of Generated by TranscodeTrackMapContext class
	// OutputTracks = ID of Output Tracks

	// Flow 2(Bypass) : Input Track -> Output Tracks
	// InputTrack = ID of Input Track
	// OutputTracks = ID of Output Tracks

	ov::String temp_debug_msg = "\r\nTranscode Pipeline \n";
	temp_debug_msg.AppendFormat(" - app(%s/%d), stream(%s/%d)\n", _application_info.GetName().CStr(), _application_info.GetId(), _input_stream->GetName().CStr(), _input_stream->GetId());

	for (auto &iter : _track_map)
	{
		auto key_pair = iter.first;
		auto &flow_context = iter.second;
		auto encode_profile_name = key_pair.first;
		auto encode_media_type = key_pair.second;

		for (auto &iter_output_tracks : flow_context->_output_tracks)
		{
			auto stream = iter_output_tracks.first;
			auto track_info = iter_output_tracks.second;

			auto input_id = flow_context->_input_track->GetId();
			auto decoder_id = flow_context->_input_track->GetId();
			auto filter_id = flow_context->_map_id;
			auto encoder_id = flow_context->_map_id;
			auto output_id = track_info->GetId();

			if (track_info->IsBypass() == true)
			{
				_stage_input_to_output[input_id].push_back(make_pair(stream, output_id));
			}
			else
			{
				// Map of InputTrack -> Decoder
				_stage_input_to_decoder[input_id] = decoder_id;

				// Map of Decoder -> Filters
				if (_stage_decoder_to_filter.find(decoder_id) == _stage_decoder_to_filter.end())
				{
					_stage_decoder_to_filter[decoder_id].push_back(filter_id);
				}
				else
				{
					auto filter_ids = _stage_decoder_to_filter[decoder_id];
					bool found = false;

					for (auto it_filter_id : filter_ids)
					{
						if (it_filter_id == filter_id)
						{
							found = true;
						}
					}

					if (found == false)
					{
						_stage_decoder_to_filter[decoder_id].push_back(filter_id);
					}
				}

				// Map of Filter -> Encoder
				_stage_filter_to_encoder[filter_id] = encoder_id;

				// Map of Encoder -> OutputTrack
				_stage_encoder_to_output[encoder_id].push_back(make_pair(stream, output_id));
			}
		}

		// for debug log
		ov::String temp_str = "";
		for (auto &iter_output_tracks : flow_context->_output_tracks)
		{
			auto stream = iter_output_tracks.first;
			auto track_info = iter_output_tracks.second;

			temp_str.AppendFormat("[%d:%s]", track_info->GetId(), (track_info->IsBypass()) ? "Bypass" : "Encode");
		}

		// for debug log
		temp_debug_msg.AppendFormat(" - Encode Profile(%s:%s) / Flow InputTrack[%d] => Decoder[%d] => Filter[%d] => Encoder[%d] => OutputTraks%s\n",
									encode_profile_name.CStr(),
									(encode_media_type == cmn::MediaType::Video) ? "Video" : "Audio",
									flow_context->_input_track->GetId(),
									flow_context->_input_track->GetId(),
									flow_context->_map_id,
									flow_context->_map_id,
									temp_str.CStr());

		created_stage_map++;
	}
	logtd(temp_debug_msg);

	return created_stage_map;
}

ov::String TranscodeStream::GetIdentifiedForVideoProfile(const cfg::vhost::app::oprf::VideoProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("_bypass_");
	}

	return ov::String::FormatString("%s-%d-%.02f-%d-%d",
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscodeStream::GetIdentifiedForImageProfile(const cfg::vhost::app::oprf::ImageProfile &profile)
{
	return ov::String::FormatString("%s-%.02f-%d-%d",
									profile.GetCodec().CStr(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscodeStream::GetIdentifiedForAudioProfile(const cfg::vhost::app::oprf::AudioProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("_bypass_");
	}

	return ov::String::FormatString("%s-%d-%d-%d",
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetSamplerate(),
									profile.GetChannel());
}

// Store information that is actually used during encoder profiles.
// This information is used to prevent encoder duplicate generation and map track IDs by stage.
void TranscodeStream::AppendTrackMap(
	ov::String unique_id,
	std::shared_ptr<info::Stream> input_stream,
	std::shared_ptr<MediaTrack> input_track,
	std::shared_ptr<info::Stream> output_stream,
	std::shared_ptr<MediaTrack> output_track)
{
	// logte("key.pair(%s, %d)", unique_id.CStr(), input_track->GetMediaType());

	auto key = std::make_pair(unique_id, input_track->GetMediaType());

	auto encoder_context_object = _track_map.find(key);
	if (encoder_context_object == _track_map.end())
	{
		auto obj = std::make_shared<TranscodeTrackMapContext>();

		obj->_map_id = _last_map_id++;
		obj->_input_stream = input_stream;
		obj->_input_track = input_track;

		obj->_output_tracks.push_back(std::make_pair(output_stream, output_track));

		_track_map[key] = obj;
	}
	else
	{
		auto obj = encoder_context_object->second;

		obj->_output_tracks.push_back(std::make_pair(output_stream, output_track));
	}
}

// Create Decoders
///
//  - Create docder by the number of tracks
// _decoders[track_id] + (transcode_context)

int32_t TranscodeStream::CreateDecoders()
{
	int32_t created_decoder_count = 0;

	for (auto iter : _stage_input_to_decoder)
	{
		auto input_track_id = iter.first;
		auto decoder_track_id = iter.second;

		auto track_item = _input_stream->GetTracks().find(input_track_id);
		if (track_item == _input_stream->GetTracks().end())
			continue;

		auto &track = track_item->second;

		std::shared_ptr<TranscodeContext> transcode_context = nullptr;

		switch (track->GetMediaType())
		{
			case cmn::MediaType::Video:
				transcode_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetWidth(),
					track->GetHeight(),
					track->GetFrameRate());
				break;

			case cmn::MediaType::Audio:
				transcode_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetSampleRate());
				break;

			default:
				logtw("Not supported media type: %d", track->GetMediaType());
				continue;
		}

		transcode_context->SetTimeBase(track->GetTimeBase());

		CreateDecoder(input_track_id, decoder_track_id, transcode_context);

		created_decoder_count++;
	}

	return created_decoder_count;
}

bool TranscodeStream::CreateDecoder(int32_t input_track_id, int32_t decoder_track_id, std::shared_ptr<TranscodeContext> input_context)
{
	// input_context must be decoding context
	OV_ASSERT2(input_context != nullptr);
	OV_ASSERT2(input_context->IsEncodingContext() == false);

	auto track = _input_stream->GetTrack(input_track_id);
	if (track == nullptr)
	{
		logte("MediaTrack not found");

		return false;
	}

	// create decoder for codec id
	auto decoder = std::move(TranscodeDecoder::CreateDecoder(*_input_stream, track->GetCodecId(), input_context));
	if (decoder == nullptr)
	{
		logte("Decoder allocation failed");
		return false;
	}

	decoder->SetTrackId(decoder_track_id);
	decoder->SetOnCompleteHandler(bind(&TranscodeStream::OnDecodedPacket, this, std::placeholders::_1, std::placeholders::_2));

	_decoders[decoder_track_id] = std::move(decoder);

	return true;
}

int32_t TranscodeStream::CreateEncoders(MediaTrackId track_id)
{
	int32_t created_encoder_count = 0;

	for (auto &[k, v] : _track_map)
	{
		if (v->_input_track->GetId() != (uint32_t)track_id)
		{
			continue;
		}

		auto encoder_track_id = v->_map_id;

		auto stage_items = _stage_encoder_to_output.find(v->_map_id);
		if (stage_items == _stage_encoder_to_output.end() || stage_items->second.size() == 0)
		{
			continue;
		}

		auto &output_track_info_item = stage_items->second[0];
		auto &output_stream = output_track_info_item.first;
		auto output_track_id = output_track_info_item.second;
		auto tracks = output_stream->GetTracks();
		auto &track = tracks[output_track_id];
		auto track_media_type = track->GetMediaType();

		switch (track_media_type)
		{
			case cmn::MediaType::Video: {
				auto encoder_context = std::make_shared<TranscodeContext>(
					true,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetWidth(),
					track->GetHeight(),
					track->GetFrameRate());

				CreateEncoder(encoder_track_id, encoder_context);
				created_encoder_count++;
			}
			break;
			case cmn::MediaType::Audio: {
				auto encoder_context = std::make_shared<TranscodeContext>(
					true,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetSampleRate());

				CreateEncoder(encoder_track_id, encoder_context);
				created_encoder_count++;
			}
			break;
			default: {
				logte("Unsuported media type. map_key:%s/%d", k.first, k.second);
			}
			break;
		}
	}

	return created_encoder_count;
}

bool TranscodeStream::CreateEncoder(int32_t encoder_track_id, std::shared_ptr<TranscodeContext> output_context)
{
	// create encoder for codec id
	auto encoder = std::move(TranscodeEncoder::CreateEncoder(output_context->GetCodecId(), output_context));
	if (encoder == nullptr)
	{
		logte("%d track encoder allication failed", encoder_track_id);
		return false;
	}

	encoder->SetTrackId(encoder_track_id);
	encoder->SetOnCompleteHandler(bind(&TranscodeStream::OnEncodedPacket, this, std::placeholders::_1));

	_encoders[encoder_track_id] = std::move(encoder);

	return true;
}

void TranscodeStream::UpdateDecoderContext(MediaTrackId track_id)
{
	if (_decoders.find(track_id) == _decoders.end())
		return;

	if (_input_stream->GetTrack(track_id) == nullptr)
		return;

	auto &decoder_context = _decoders[track_id]->GetContext();
	auto &track = _input_stream->GetTrack(track_id);

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

// Update Track of Input Stream
void TranscodeStream::UpdateInputTrack(MediaFrame *buffer)
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
		input_track->GetChannel().SetLayout(buffer->GetChannelLayout());
	}
}

// Update Output Track
void TranscodeStream::UpdateOutputTrack(MediaFrame *buffer)
{
	for (auto &[k, v] : _track_map)
	{
		(void)(k);

		if (buffer->GetTrackId() != (int32_t)v->_input_track->GetId())
		{
			continue;
		}

		for (auto &o : v->_output_tracks)
		{
			// OutputStream->MediaTrack
			auto &output_track = o.second;

			if (output_track->IsBypass() == true)
			{
				output_track->SetWidth(buffer->GetWidth());
				output_track->SetHeight(buffer->GetHeight());
				output_track->SetFormat(buffer->GetFormat());
				output_track->SetSampleRate(buffer->GetSampleRate());
				output_track->GetSample().SetFormat(buffer->GetFormat<cmn::AudioSample::Format>());
				output_track->GetChannel().SetLayout(buffer->GetChannelLayout());
			}
			else
			{
				if (output_track->GetWidth() == 0)
				{
					output_track->SetWidth(buffer->GetWidth());
				}
				if (output_track->GetHeight() == 0)
				{
					output_track->SetHeight(buffer->GetHeight());
				}
				if (output_track->GetFormat() == 0)
				{
					output_track->SetFormat(buffer->GetFormat());
				}
			}

			// Exception : If there is no framerate information, fix it at any value.
			if (output_track->GetFrameRate() == 0)
			{
				output_track->SetFrameRate(1.0f);
			}
		}
	}
}

void TranscodeStream::ChangeOutputFormat(MediaFrame *buffer)
{
	if (buffer == nullptr)
	{
		logte("Invalid media buffer");
		return;
	}

	// Update Track of Input Stream
	UpdateInputTrack(buffer);

	// Update Decoder Context
	UpdateDecoderContext(buffer->GetTrackId());

	// Udpate Track of Output Stream
	UpdateOutputTrack(buffer);

	// Create Encoder
	CreateEncoders(buffer->GetTrackId());

	// Craete Filter
	CreateFilter(buffer);
}

void TranscodeStream::DecodePacket(int32_t track_id, std::shared_ptr<MediaPacket> packet)
{
	// 1. bypass track processing
	auto stage_item_to_output = _stage_input_to_output.find(track_id);
	if (stage_item_to_output != _stage_input_to_output.end())
	{
		auto &output_tracks = stage_item_to_output->second;

		for (auto iter : output_tracks)
		{
			auto output_stream = iter.first;
			auto output_track_id = iter.second;

			auto input_track = _input_stream->GetTrack(track_id);
			if (input_track == nullptr)
			{
				logte("Could not found input track. track_id(%d)", track_id);
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

			double scale = input_track->GetTimeBase().GetExpr() / output_track->GetTimeBase().GetExpr();
			clone_packet->SetPts((int64_t)((double)clone_packet->GetPts() * scale));
			clone_packet->SetDts((int64_t)((double)clone_packet->GetDts() * scale));

			SendFrame(output_stream, std::move(clone_packet));
		}
	}

	// 2. decoding track processing
	auto stage_item_decoder = _stage_input_to_decoder.find(track_id);
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
	auto decoder = decoder_item->second.get();

	logtp("[#%3d] Decode In.  PTS: %lld, SIZE: %lld",
		  track_id,
		  (int64_t)(packet->GetPts() * decoder->GetTimebase().GetExpr() * 1000),
		  packet->GetDataLength());

	decoder->SendBuffer(std::move(packet));
}

void TranscodeStream::OnDecodedPacket(TranscodeResult result, int32_t decoder_id)
{
	auto decoder_item = _decoders.find(decoder_id);
	if (decoder_item == _decoders.end())
	{
		return;
	}

	auto decoder = decoder_item->second.get();
	TranscodeResult unused;
	auto decoded_frame = decoder->RecvBuffer(&unused);
	if (decoded_frame == nullptr)
	{
		return;
	}

	switch (result)
	{
		case TranscodeResult::FormatChanged:
			// It indicates output format is changed

			// Re-create filter and encoder using the format
			decoded_frame->SetTrackId(decoder_id);
			ChangeOutputFormat(decoded_frame.get());

			[[fallthrough]];

		case TranscodeResult::DataReady:
			decoded_frame->SetTrackId(decoder_id);

			logtp("[#%3d] Decode Out. PTS: %lld, SIZE: %lld, DURATION: %lld",
				  decoder_id,
				  (int64_t)(decoded_frame->GetPts() * decoder->GetTimebase().GetExpr() * 1000),
				  (int64_t)decoded_frame->GetBufferSize(),
				  (int64_t)((double)decoded_frame->GetDuration() * decoder->GetTimebase().GetExpr() * 1000));

			SpreadToFilters(std::move(decoded_frame));

			break;
		default:
			// An error occurred
			// There is no frame to process
			break;
	}
}

TranscodeResult TranscodeStream::FilterFrame(int32_t track_id, std::shared_ptr<MediaFrame> decoded_frame)
{
	auto filter_item = _filters.find(track_id);
	if (filter_item == _filters.end())
	{
		return TranscodeResult::NoData;
	}

	auto filter = filter_item->second.get();

	logtp("[#%3d] Filter In.  PTS: %lld, SIZE: %lld",
		  track_id,
		  (int64_t)(decoded_frame->GetPts() * filter->GetInputTimebase().GetExpr() * 1000),
		  decoded_frame->GetBufferSize());

	filter->SendBuffer(std::move(decoded_frame));

	while (true)
	{
		TranscodeResult result;
		auto filtered_frame = filter->RecvBuffer(&result);

		if (static_cast<int>(result) < 0)
		{
			return result;
		}

		switch (result)
		{
			case TranscodeResult::DataReady: {
				filtered_frame->SetTrackId(track_id);

				logtp("[#%3d] Filter Out. PTS: %lld, SIZE: %lld",
					  track_id,
					  (int64_t)(filtered_frame->GetPts() * filter->GetOutputTimebase().GetExpr() * 1000),
					  filtered_frame->GetBufferSize());

				int32_t filter_id = filtered_frame->GetTrackId();
				EncodeFrame(filter_id, std::move(filtered_frame));
			}
			break;

			default:
				return result;
		}
	}
}

TranscodeResult TranscodeStream::EncodeFrame(int32_t filter_id, std::shared_ptr<const MediaFrame> frame)
{
	auto encoder_id = _stage_filter_to_encoder[filter_id];

	auto encoder_item = _encoders.find(encoder_id);
	if (encoder_item == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_item->second.get();

	logtp("[#%3d] Encode In.  PTS: %lld, FLAGS: %d, SIZE: %d",
		  encoder_id,
		  (int64_t)(frame->GetPts() * encoder->GetTimebase().GetExpr() * 1000),
		  frame->GetFlags(),
		  frame->GetBufferSize());

	encoder->SendBuffer(std::move(frame));

	return TranscodeResult::NoData;
}

// Callback is called from the encoder for packets that have been encoded.
// @setEncodedHandler
TranscodeResult TranscodeStream::OnEncodedPacket(int32_t encoder_id)
{
	auto encoder_item = _encoders.find(encoder_id);
	if (encoder_item == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_item->second.get();

	while (true)
	{
		TranscodeResult result;

		auto encoded_packet = encoder->RecvBuffer(&result);

		if (static_cast<int>(result) < 0)
		{
			return result;
		}

		if (result == TranscodeResult::DataReady)
		{
			logtp("[#%3d] Encode Out. PTS: %lld, FLAGS: %d, SIZE: %d",
				  encoder_id,
				  (int64_t)(encoded_packet->GetPts() * encoder->GetTimebase().GetExpr() * 1000),
				  encoded_packet->GetFlag(),
				  encoded_packet->GetDataLength());

			// Explore if output tracks exist to send encoded packets
			auto stage_item = _stage_encoder_to_output.find(encoder_id);
			if (stage_item == _stage_encoder_to_output.end())
			{
				continue;
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
	}

	return TranscodeResult::NoData;
}

void TranscodeStream::NotifyCreateStreams()
{
	for (auto &iter : _output_streams)
	{
		auto stream_output = iter.second;

		bool ret = _parent->CreateStream(stream_output);
		if (ret == false)
		{
			// TODO(soulk): If the stream creation fails, an exception must be processed.
		}
	}
}

void TranscodeStream::NotifyDeleteStreams()
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

void TranscodeStream::SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet)
{
	bool ret = _parent->SendFrame(stream, std::move(packet));
	if (ret == false)
	{
		// TODO(soulk): If SendFrame fails, an exception must be processed.
	}
}

void TranscodeStream::CreateFilter(MediaFrame *buffer)
{
	MediaTrackId track_id = buffer->GetTrackId();

	// due to structural problems I've already made the transcode's context... so, update changed data.
	auto &input_track = _input_stream->GetTrack(track_id);
	auto &input_transcode_context = _decoders[track_id]->GetContext();

	// 3. Get Output Track List. Creates a filter by looking up the encoding context information of the output track.
	auto filter_item = _stage_decoder_to_filter.find(track_id);
	if (filter_item == _stage_decoder_to_filter.end())
	{
		logtw("Filter not found");
		return;
	}
	auto filter_id_list = filter_item->second;

	for (auto &filter_id : filter_id_list)
	{
		auto encoder_id = _stage_filter_to_encoder[filter_id];

		if (_encoders.find(encoder_id) == _encoders.end())
		{
			logte("%d track encoder is not allocated", encoder_id);
			continue;
		}

		auto output_transcode_context = _encoders[encoder_id]->GetContext();

		auto transcode_filter = std::make_shared<TranscodeFilter>();

		bool ret = transcode_filter->Configure(input_track, input_transcode_context, output_transcode_context);
		if (ret == true)
		{
			_filters[filter_id] = transcode_filter;
		}
		else
		{
			// TODO(soulk) : Create exception processing code if filter creation fails
			logte("Failed to create filter");
		}
	}
}

void TranscodeStream::SpreadToFilters(std::shared_ptr<MediaFrame> frame)
{
	// Get decode id
	int32_t decoder_id = frame->GetTrackId();

	// Query filter list to forward decode frame
	auto filter_item = _stage_decoder_to_filter.find(decoder_id);
	if (filter_item == _stage_decoder_to_filter.end())
	{
		logtw("No filter list found");
		return;
	}

	for (auto &filter_id : filter_item->second)
	{
		auto frame_clone = frame->CloneFrame();
		if (frame_clone == nullptr)
		{
			logte("FilterTask -> Unknown Frame");
			continue;
		}

		FilterFrame(filter_id, std::move(frame_clone));
	}
}

uint8_t TranscodeStream::NewTrackId(cmn::MediaType media_type)
{
	uint8_t last_index = 0;

	last_index = _last_track_index;
	_last_track_index++;

	return last_index;
}

cmn::MediaCodecId TranscodeStream::GetCodecId(ov::String name)
{
	name.MakeUpper();

	// Video codecs
	if (name == "H264")
	{
		return cmn::MediaCodecId::H264;
	}
	else if (name == "H265")
	{
		return cmn::MediaCodecId::H265;
	}
	else if (name == "VP8")
	{
		return cmn::MediaCodecId::Vp8;
	}
	else if (name == "VP9")
	{
		return cmn::MediaCodecId::Vp9;
	}
	else if (name == "JPEG")
	{
		return cmn::MediaCodecId::Jpeg;
	}
	else if (name == "PNG")
	{
		return cmn::MediaCodecId::Png;
	}

	// Audio codecs
	if (name == "AAC")
	{
		return cmn::MediaCodecId::Aac;
	}
	else if (name == "MP3")
	{
		return cmn::MediaCodecId::Mp3;
	}
	else if (name == "OPUS")
	{
		return cmn::MediaCodecId::Opus;
	}

	return cmn::MediaCodecId::None;
}

bool TranscodeStream::IsVideoCodec(cmn::MediaCodecId codec_id)
{
	if (codec_id == cmn::MediaCodecId::H264 ||
		codec_id == cmn::MediaCodecId::H265 ||
		codec_id == cmn::MediaCodecId::Vp8 ||
		codec_id == cmn::MediaCodecId::Vp9 ||
		codec_id == cmn::MediaCodecId::Jpeg ||
		codec_id == cmn::MediaCodecId::Png)
	{
		return true;
	}

	return false;
}

bool TranscodeStream::IsAudioCodec(cmn::MediaCodecId codec_id)
{
	if (codec_id == cmn::MediaCodecId::Aac ||
		codec_id == cmn::MediaCodecId::Mp3 ||
		codec_id == cmn::MediaCodecId::Opus)
	{
		return true;
	}

	return false;
}
