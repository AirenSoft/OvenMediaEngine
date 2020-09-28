//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_application.h"
#include "transcode_stream.h"

#include <config/config_manager.h>

#define OV_LOG_TAG "TranscodeStream"

TranscodeStream::TranscodeStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _application_info(application_info),
	_queue_input_packets(nullptr, 100),
	_queue_decoded_frames(nullptr, 100),
	_queue_filterd_frames(nullptr, 100)
{
	logtd("Trying to create transcode stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	// Determine maximum queue size
	_max_queue_threshold = 0;

	//store Parent information
	_parent = parent;

	// Store Stream information
	_stream_input = stream;

	// for generating track ids
	_last_transcode_id = 0;

	// set alias
	_queue_input_packets.SetAlias(ov::String::FormatString("%s/%s - TranscodeStream input Queue"
		, _stream_input->GetApplicationInfo().GetName().CStr() ,_stream_input->GetName().CStr()));

	_queue_decoded_frames.SetAlias(ov::String::FormatString("%s/%s - TranscodeStream decoded Queue"
		, _stream_input->GetApplicationInfo().GetName().CStr() ,_stream_input->GetName().CStr()));

	_queue_filterd_frames.SetAlias(ov::String::FormatString("%s/%s - TranscodeStream filtered Queue"
		, _stream_input->GetApplicationInfo().GetName().CStr() ,_stream_input->GetName().CStr()));

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

	// Delete all data in the queue
	_queue_input_packets.Clear();
	_queue_decoded_frames.Clear();
	_queue_filterd_frames.Clear();

	// Delete all map of stage
	_stage_input_to_decoder.clear();
	_stage_input_to_output.clear();
	_stage_decoder_to_filter.clear();
	_stage_filter_to_encoder.clear();
	_stage_encoder_to_output.clear();

	_stream_outputs.clear();
}

TranscodeApplication* TranscodeStream::GetParent()
{
	return _parent;
}

bool TranscodeStream::Start()
{			
	if (CreateOutputStream() == 0)
	{
		logte("No output stream generated");
		return false;
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

	if (CreateEncoders() == 0)
	{
		logti("No encoder generated");
	}

	// I will make and apply a packet drop policy.
	_max_queue_threshold = 256;

	_kill_flag = false;

	// Notify to create a new stream on the media router.
	CreateStreams();

	logti("[%s/%s(%u)] Transcoder input stream has been started. Status : (%d) Decoders, (%d) Encoders", 
						_application_info.GetName().CStr(), _stream_input->GetName().CStr(), _stream_input->GetId(), _decoders.size(), _encoders.size());
			
	return true;
}

bool TranscodeStream::Stop()
{
	_kill_flag = true;

	logtd("Wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag ? "true" : "false");


	// Stop all queues
	_queue_input_packets.Stop();
	_queue_decoded_frames.Stop();
	_queue_filterd_frames.Stop();

	// Stop all encoders
	for (auto &iter : _encoders)
	{
		auto object = iter.second;
		object->Stop();
		object.reset();
	}

	// Stop all filters
	for (auto &iter : _filters)
	{
		auto object = iter.second;

		object.reset();
	}

	// Stop all decoder
	for (auto &iter : _decoders)
	{
		auto object = iter.second;

		object.reset();
	}



	// Notify to delete the stream created on the MediaRouter
	DeleteStreams();

	logti("[%s/%s(%u)] Transcoder input stream has been stopped."
		,_application_info.GetName().CStr(), _stream_input->GetName().CStr(), _stream_input->GetId());

	return true;
}

bool TranscodeStream::Push(std::shared_ptr<MediaPacket> packet)
{
	if (_max_queue_threshold == 0)
	{
		// Drop packets because the transcoding information has not been generated.
		return true;
	}

	if (_queue_input_packets.Size() > _max_queue_threshold)
	{
		logti("Queue(stream) is full, please check your system: (queue: %zu > limit: %llu)", _queue_input_packets.Size(), _max_queue_threshold);
		return false;
	}

	_queue_input_packets.Enqueue(std::move(packet));

	if(GetParent() != nullptr)
		GetParent()->AppendIndicator(this->GetSharedPtr(), TranscodeApplication::IndicatorQueueType::BUFFER_INDICATOR_INPUT_PACKETS);

	return true;
}

// Create Output Stream and Encoding Transcode Context
int32_t TranscodeStream::CreateOutputStream()
{
	int32_t created_stream_count = 0;

	// If the application is created by Dynamic, make it bypass in Default Stream.
	if( _application_info.IsDynamicApp() == true )
	{
		auto stream_output = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);

		stream_output->SetName(_stream_input->GetName());
		stream_output->SetOriginStream(_stream_input);

		for (auto &input_track_item : _stream_input->GetTracks())
		{
			auto &input_track = input_track_item.second;
			auto input_track_media_type = input_track->GetMediaType();

			auto new_outupt_track = std::make_shared<MediaTrack>();

			new_outupt_track->SetBypass(true);
			new_outupt_track->SetId(NewTrackId(new_outupt_track->GetMediaType()));
			new_outupt_track->SetBitrate(input_track->GetBitrate());
			new_outupt_track->SetCodecId(input_track->GetCodecId());

			if (input_track_media_type == common::MediaType::Video)
			{
				new_outupt_track->SetMediaType(common::MediaType::Video);
				new_outupt_track->SetWidth(input_track->GetWidth());
				new_outupt_track->SetHeight(input_track->GetHeight());
				new_outupt_track->SetFrameRate(input_track->GetFrameRate());
				new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());

				stream_output->AddTrack(new_outupt_track);
				StoreStageContext("default", input_track_media_type, input_track, stream_output, new_outupt_track);
			}
			else if (input_track_media_type == common::MediaType::Audio)
			{
				new_outupt_track->SetMediaType(common::MediaType::Audio);
				auto input_codec_id = input_track->GetCodecId();
				auto input_samplerate = input_track->GetSampleRate();

				if (input_codec_id == common::MediaCodecId::Opus)
				{
					if (input_samplerate != 48000)
					{
						logtw("OPUS codec only supports 48000Hz samplerate. Do not create bypass track. input smplereate(%d)", input_samplerate);
						continue;
					}
				}

				// Set output specification
				new_outupt_track->SetSampleRate(input_samplerate);
				new_outupt_track->GetChannel().SetLayout(input_track->GetChannel().GetLayout());
				new_outupt_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
				new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());	

				stream_output->AddTrack(new_outupt_track);
				StoreStageContext("default", input_track_media_type, input_track, stream_output, new_outupt_track);
			}
		}


		// Add to Output Stream List. The key is the output stream name.
		_stream_outputs.insert(std::make_pair(stream_output->GetName(), stream_output));

		logti("[%s/%s(%u)] -> [%s/%s(%u)] Transcoder output stream has been created.", 
						_application_info.GetName().CStr(), _stream_input->GetName().CStr(), _stream_input->GetId(),
						_application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());

		// Number of generated output streams
		created_stream_count++;		

		return created_stream_count;
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. Create new stream and new track
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Get [application->Streams] list of application configuration.
	auto &cfg_stream_list = _application_info.GetConfig().GetStreamList();

	for (const auto &cfg_stream : cfg_stream_list)
	{
		// Validation 
		if (cfg_stream.GetName().GetLength() == 0)
		{
			logte("there is no stream name");
			continue;
		}

		auto stream_output = std::make_shared<info::Stream>(_application_info, StreamSourceType::Transcoder);

		// Create a new stream name.
		auto stream_name = cfg_stream.GetName();
		if (::strstr(stream_name.CStr(), "${OriginStreamName}") != nullptr)
		{
			stream_name = stream_name.Replace("${OriginStreamName}", _stream_input->GetName());
		}
		stream_output->SetName(stream_name);

		// It helps modules to reconize origin stream from provider
		stream_output->SetOriginStream(_stream_input);

		// Look up all tracks in the input stream.
		for (auto &input_track_item : _stream_input->GetTracks())
		{
			auto &input_track = input_track_item.second;
			auto input_track_media_type = input_track->GetMediaType();

			// Create a new output media tracks.
			auto &cfg_profiles = cfg_stream.GetProfileList();
			for (const auto &cfg_profile : cfg_profiles)
			{
				// Gets the encode profile information from the application configruation.
				auto cfg_encode = GetEncodeByProfileName(_application_info, cfg_profile.GetName());
				if (cfg_encode == nullptr)
				{
					logtw("There is no encode profile. input track(%d), name(%s)", input_track_item.first, cfg_profile.GetName().CStr());
					continue;
				}

				if (input_track_media_type != common::MediaType::Video && input_track_media_type != common::MediaType::Audio)
				{
					logtw("Unsupported media type of input track. type(%d)", input_track_media_type);
					continue;
				}

				auto new_outupt_track = std::make_shared<MediaTrack>();

				if (input_track_media_type == common::MediaType::Video)
				{
					auto cfg_encode_video = cfg_encode->GetVideoProfile();

					if ((cfg_encode_video != nullptr) && (cfg_encode_video->IsActive()))
					{
						new_outupt_track->SetBypass(cfg_encode_video->IsBypass());
						new_outupt_track->SetId(NewTrackId(new_outupt_track->GetMediaType()));
						new_outupt_track->SetMediaType(common::MediaType::Video);

						// When bypass is enabled, it gets information from tracks in the input stream.
						if (new_outupt_track->IsBypass() == true)
						{
							// Validation

							// Set output specification
							new_outupt_track->SetCodecId(input_track->GetCodecId());
							new_outupt_track->SetBitrate(input_track->GetBitrate());
							new_outupt_track->SetWidth(input_track->GetWidth());
							new_outupt_track->SetHeight(input_track->GetHeight());
							new_outupt_track->SetFrameRate(input_track->GetFrameRate());
							new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							auto output_codec_id = GetCodecId(cfg_encode_video->GetCodec());
							auto output_bitrate = GetBitrate(cfg_encode_video->GetBitrate());
							auto output_width = cfg_encode_video->GetWidth();
							auto output_height = cfg_encode_video->GetHeight();
							auto output_framerate = cfg_encode_video->GetFramerate();

							// Validation
							if(IsVideoCodec(output_codec_id) == false)
							{
								logtw("Encoding codec set is not a video codec");
								continue;
							}

							// Set output specification
							new_outupt_track->SetCodecId(output_codec_id);
							new_outupt_track->SetBitrate(output_bitrate);
							new_outupt_track->SetWidth(output_width);
							new_outupt_track->SetHeight(output_height);
							new_outupt_track->SetFrameRate(output_framerate);

							// The timebase value will change by the decoder event.
							new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}

						stream_output->AddTrack(new_outupt_track);

						StoreStageContext(cfg_profile.GetName(), input_track_media_type, input_track, stream_output, new_outupt_track);
					}
				}
				else if (input_track_media_type == common::MediaType::Audio)
				{
					auto cfg_encode_audio = cfg_encode->GetAudioProfile();

					if ((cfg_encode_audio != nullptr) && (cfg_encode_audio->IsActive()))
					{
						new_outupt_track->SetId(NewTrackId(new_outupt_track->GetMediaType()));
						new_outupt_track->SetMediaType(common::MediaType::Audio);
						new_outupt_track->SetBypass(cfg_encode_audio->IsBypass());

						if (new_outupt_track->IsBypass() == true)
						{
							// Validation
							auto input_codec_id = input_track->GetCodecId();
							auto input_samplerate = input_track->GetSampleRate();

							if (input_codec_id == common::MediaCodecId::Opus)
							{
								if (input_samplerate != 48000)
								{
									logtw("OPUS codec only supports 48000Hz samplerate. Do not create bypass track. input smplereate(%d)", input_samplerate);
									continue;
								}
							}

							// Set output specification
							new_outupt_track->SetCodecId(input_codec_id);
							new_outupt_track->SetBitrate(input_track->GetBitrate());
							new_outupt_track->SetSampleRate(input_samplerate);
							new_outupt_track->GetChannel().SetLayout(input_track->GetChannel().GetLayout());
							new_outupt_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
							new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							// Validation
							auto output_codec_id = GetCodecId(cfg_encode_audio->GetCodec());
							auto output_samplerate = cfg_encode_audio->GetSamplerate();
							auto output_bitrate = GetBitrate(cfg_encode_audio->GetBitrate());
							auto output_channel_layout = cfg_encode_audio->GetChannel() == 1 ? common::AudioChannel::Layout::LayoutMono : common::AudioChannel::Layout::LayoutStereo;

							if (output_codec_id == common::MediaCodecId::Opus)
							{
								if (output_samplerate != 48000)
								{
									logtw("OPUS codec only supports 48000Hz samplerate. change the samplerate to 48000Hz");
									output_samplerate = 48000;
								}
							}

							if(IsAudioCodec(output_codec_id) == false)
							{
								logtw("Encoding codec set is not a audio codec");
								continue;
							}

							// Set output specification
							new_outupt_track->SetCodecId(output_codec_id);
							new_outupt_track->SetSampleRate(output_samplerate);
							new_outupt_track->SetBitrate(output_bitrate);
							new_outupt_track->GetChannel().SetLayout(output_channel_layout);
							new_outupt_track->SetTimeBase(1, output_samplerate);

							// The sample format will change by the decoder event.
							new_outupt_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
						}

						stream_output->AddTrack(new_outupt_track);

						StoreStageContext(cfg_profile.GetName(), input_track_media_type, input_track, stream_output, new_outupt_track);
					}
				}
			}
		}

		// Add to Output Stream List. The key is the output stream name.
		_stream_outputs.insert(std::make_pair(stream_name, stream_output));

		logti("[%s/%s(%u)] -> [%s/%s(%u)] Transcoder output stream has been created.", 
						_application_info.GetName().CStr(), _stream_input->GetName().CStr(), _stream_input->GetId(),
						_application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());

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
	// Filter = ID of Generated by TranscodeStageContext class
	// Encoder = ID of Generated by TranscodeStageContext class
	// OutputTracks = ID of Output Tracks

	// Flow 2(Bypass) : Input Track -> Output Tracks
	// InputTrack = ID of Input Track
	// OutputTracks = ID of Output Tracks

	ov::String temp_debug_msg = "\r\nStage Map of Transcoder\n";
	temp_debug_msg.AppendFormat(" - app(%s/%d), stream(%s/%d)\n", _application_info.GetName().CStr(), _application_info.GetId(), _stream_input->GetName().CStr(), _stream_input->GetId());

	for (auto &iter : _map_stage_context)
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
			auto filter_id = flow_context->_transcoder_id;
			auto encoder_id = flow_context->_transcoder_id;
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

			temp_str.AppendFormat("[%d:%s]", track_info->GetId(), (track_info->IsBypass()) ? "Pss" : "Enc");
		}

		// for debug log
		temp_debug_msg.AppendFormat(" - Encode Profile(%s:%s) / Flow InputTrack[%d] => Decoder[%d] => Filter[%d] => Encoder[%d] => OutputTraks%s\n", encode_profile_name.CStr(), (encode_media_type == common::MediaType::Video) ? "Video" : "Audio", flow_context->_input_track->GetId(), flow_context->_input_track->GetId(), flow_context->_transcoder_id, flow_context->_transcoder_id, temp_str.CStr());

		created_stage_map++;
	}
	logtd(temp_debug_msg);

	return created_stage_map;
}

// Store information that is actually used during encoder profiles. This information is used to prevent encoder duplicate generation and map track IDs by stage.
void TranscodeStream::StoreStageContext(ov::String encode_profile_name, common::MediaType media_type, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	auto key = std::make_pair(encode_profile_name, media_type);

	auto encoder_context_object = _map_stage_context.find(key);
	if (encoder_context_object == _map_stage_context.end())
	{
		auto obj = std::make_shared<TranscodeStageContext>();
		obj->_transcoder_id = _last_transcode_id++;
		obj->_input_track = input_track;
		obj->_output_tracks.push_back(std::make_pair(output_stream, output_track));

		_map_stage_context[key] = obj;
	}
	else
	{
		auto obj = encoder_context_object->second;

		obj->_output_tracks.push_back(std::make_pair(output_stream, output_track));
	}
}

// Create Docoders
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

		auto track_item = _stream_input->GetTracks().find(input_track_id);
		if (track_item == _stream_input->GetTracks().end())
			continue;

		auto &track = track_item->second;

		std::shared_ptr<TranscodeContext> input_context = nullptr;

		switch (track->GetMediaType())
		{
			case common::MediaType::Video:
				input_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetWidth(), track->GetHeight(),
					track->GetFrameRate());
				break;

			case common::MediaType::Audio:
				input_context = std::make_shared<TranscodeContext>(
					false,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetSampleRate());
				break;

			default:
				logtw("Not supported media type: %d", track->GetMediaType());
				continue;
		}

		input_context->SetTimeBase(track->GetTimeBase());

		CreateDecoder(input_track_id, decoder_track_id, input_context);

		created_decoder_count++;
	}

	return created_decoder_count;
}

bool TranscodeStream::CreateDecoder(int32_t input_track_id, int32_t decoder_track_id, std::shared_ptr<TranscodeContext> input_context)
{
	// input_context must be decoding context
	OV_ASSERT2(input_context != nullptr);
	OV_ASSERT2(input_context->IsEncodingContext() == false);

	auto track = _stream_input->GetTrack(input_track_id);
	if (track == nullptr)
	{
		logte("MediaTrack not found");

		return false;
	}

	// create decoder for codec id
	auto decoder = std::move(TranscodeDecoder::CreateDecoder(*_stream_input, track->GetCodecId(), input_context));
	if (decoder == nullptr)
	{
		logte("Decoder allocation failed");
		return false;
	}

	_decoders[decoder_track_id] = std::move(decoder);

	return true;
}

int32_t TranscodeStream::CreateEncoders()
{
	int32_t created_encoder_count = 0;

	for (auto &iter : _map_stage_context)
	{
		auto &flow_context = iter.second;
		auto encoder_track_id = flow_context->_transcoder_id;

		auto stage_items = _stage_encoder_to_output.find(encoder_track_id);
		if (stage_items == _stage_encoder_to_output.end())
		{
			continue;
		}

		if (stage_items->second.size() == 0)
		{
			logtw("Encoder is not required");
			continue;
		}

		// Gets the information of the first track of the output tracks.
		auto &output_track_info_item = stage_items->second[0];

		auto &output_stream = output_track_info_item.first;
		auto output_track_id = output_track_info_item.second;
		auto tracks = output_stream->GetTracks();
		auto &track = tracks[output_track_id];
		auto track_media_type = track->GetMediaType();

		switch (track_media_type)
		{
			case common::MediaType::Video:
			{
				// TODO(soulk): Addicational parameters should be set.
				//	- Encoding profile level
				//	- etc
				auto new_output_transcode_context = std::make_shared<TranscodeContext>(
					true,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetWidth(),
					track->GetHeight(),
					track->GetFrameRate());

				CreateEncoder(encoder_track_id, track, new_output_transcode_context);
				created_encoder_count++;
			}
			break;
			case common::MediaType::Audio:
			{
				// TODO(soulk): Addicational parameters should be set.
				//  - Channel Layout
				//	- etc
				auto new_output_transcode_context = std::make_shared<TranscodeContext>(
					true,
					track->GetCodecId(),
					track->GetBitrate(),
					track->GetSampleRate());

				CreateEncoder(encoder_track_id, track, new_output_transcode_context);
				created_encoder_count++;
			}
			break;
			default:
			{
				logte("Unsuported media type");
			}
			break;
		}
	}

	return created_encoder_count;
}

bool TranscodeStream::CreateEncoder(int32_t encoder_track_id, std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context)
{
	if (media_track == nullptr)
	{
		logte("Invalid MediaTrack");
		return false;
	}

	// create encoder for codec id
	auto encoder = std::move(TranscodeEncoder::CreateEncoder(media_track->GetCodecId(), output_context));
	if (encoder == nullptr)
	{
		logte("%d track encoder allication failed", encoder_track_id);
		return false;
	}

	encoder->SetTrackId(encoder_track_id);
	encoder->SetOnCompleteHandler(bind(&TranscodeStream::EncodedPacket, this, std::placeholders::_1));

	_encoders[encoder_track_id] = std::move(encoder);

	return true;
}

void TranscodeStream::ChangeOutputFormat(MediaFrame *buffer)
{
	if (buffer == nullptr)
	{
		logte("Invalid media buffer");
		return;
	}

	CreateFilters(buffer);
}

TranscodeResult TranscodeStream::DecodePacket(int32_t track_id, std::shared_ptr<MediaPacket> packet)
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

			auto clone_packet = packet->ClonePacket();

			clone_packet->SetTrackId(output_track_id);

			// if(output_track_id == 1)
			// logtd("[#%d] Passthrought to MediaRouter (PTS: %lld)(SIZE: %lld)", output_track_id, clone_packet->GetPts(), clone_packet->GetDataLength());

			SendFrame(output_stream, std::move(clone_packet));
		}
	}


	// 2. decoding track processing
	auto stage_item_decoder = _stage_input_to_decoder.find(track_id);
	if (stage_item_decoder == _stage_input_to_decoder.end())
	{
		return TranscodeResult::NoData;
	}

	auto decoder_id = stage_item_decoder->second;

	auto decoder_item = _decoders.find(decoder_id);
	if (decoder_item == _decoders.end())
	{
		return TranscodeResult::NoData;
	}
	auto decoder = decoder_item->second.get();

	logtp("[#%3d] Decode In.  PTS: %lld, SIZE: %lld", 
		track_id, 
		(int64_t)(packet->GetPts() * decoder->GetTimebase().GetExpr()*1000), 
		packet->GetDataLength());

	decoder->SendBuffer(std::move(packet));

	while (true)
	{
		TranscodeResult result;
		auto decoded_frame = decoder->RecvBuffer(&result);

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

				logtp("[#%3d] Decode Out. PTS: %lld, SIZE: %lld", 
					decoder_id, 
					(int64_t)(decoded_frame->GetPts() * decoder->GetTimebase().GetExpr()*1000), 
					decoded_frame->GetBufferSize() );

				if (_queue_decoded_frames.Size() > _max_queue_threshold)
				{
					logti("Decoded frame queue is full, please check your system");
					return result;
				}

				_queue_decoded_frames.Enqueue(std::move(decoded_frame));

				if(GetParent() != nullptr)
					GetParent()->AppendIndicator(this->GetSharedPtr(), TranscodeApplication::IndicatorQueueType::BUFFER_INDICATOR_DECODED_FRAMES);

				continue;

			case TranscodeResult::DataError:
			case TranscodeResult::NoData:
			default:
				// An error occurred
				// There is no frame to process
				return result;
		}
	}

	return TranscodeResult::NoData;
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
			(int64_t)(decoded_frame->GetPts() * filter->GetInputTimebase().GetExpr()*1000), 
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
			case TranscodeResult::DataReady:
				filtered_frame->SetTrackId(track_id);

				logtp("[#%3d] Filter Out. PTS: %lld, SIZE: %lld", 
					track_id, 
					(int64_t)(filtered_frame->GetPts()* filter->GetOutputTimebase().GetExpr()*1000), 
					filtered_frame->GetBufferSize());

				_queue_filterd_frames.Enqueue(std::move(filtered_frame));

				if(GetParent() != nullptr)
					GetParent()->AppendIndicator(this->GetSharedPtr(), TranscodeApplication::IndicatorQueueType::BUFFER_INDICATOR_FILTERED_FRAMES);

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
		(int64_t)(frame->GetPts() * encoder->GetTimebase().GetExpr()*1000), 
		frame->GetFlags(),
		frame->GetBufferSize());

	encoder->SendBuffer(std::move(frame));

	return TranscodeResult::NoData;	
}

// Callback is called from the encoder for packets that have been encoded.
// @setEncodedHandler
TranscodeResult TranscodeStream::EncodedPacket(int32_t encoder_id)
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
				(int64_t)(encoded_packet->GetPts() * encoder->GetTimebase().GetExpr()*1000), 
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

void TranscodeStream::DoInputPackets()
{
	auto packet = _queue_input_packets.Dequeue();
	if (packet.has_value())
	{
		int32_t track_id = packet.value()->GetTrackId();

		DecodePacket(track_id, std::move(packet.value()));
	}
}


void TranscodeStream::DoDecodedFrames()
{
	auto frame = _queue_decoded_frames.Dequeue();
	if (frame.has_value())
	{
		DoFilters(std::move(frame.value()));
	}
}

void TranscodeStream::DoFilteredFrames()
{
	auto frame = _queue_filterd_frames.Dequeue();
	if (frame.has_value())
	{
		int32_t filter_id = frame.value()->GetTrackId();

		EncodeFrame(filter_id, std::move(frame.value()));
	}
}

void TranscodeStream::CreateStreams()
{
	for (auto &iter : _stream_outputs)
	{
		auto stream_output = iter.second;

		_parent->CreateStream(stream_output);
	}
}

void TranscodeStream::DeleteStreams()
{
	for (auto &iter : _stream_outputs)
	{
		auto stream_output = iter.second;
		logti("[%s/%s(%u)] -> [%s/%s(%u)] Transcoder output stream has been deleted.", 
						_application_info.GetName().CStr(), _stream_input->GetName().CStr(), _stream_input->GetId(),
						_application_info.GetName().CStr(), stream_output->GetName().CStr(), stream_output->GetId());

		_parent->DeleteStream(stream_output);
	}

	_stream_outputs.clear();
}

void TranscodeStream::SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet)
{
	_parent->SendFrame(stream, std::move(packet));
}

void TranscodeStream::CreateFilters(MediaFrame *buffer)
{
	MediaTrackId input_id = buffer->GetTrackId();
	MediaTrackId decoder_id = buffer->GetTrackId();

	// 1. Update information for input media track from decoded media frame.
	auto &input_media_track = _stream_input->GetTrack(input_id);
	if (input_media_track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", input_id);

		return;
	}

	if (input_media_track->GetMediaType() == common::MediaType::Video)
	{
		input_media_track->SetWidth(buffer->GetWidth());
		input_media_track->SetHeight(buffer->GetHeight());
		input_media_track->SetFormat(buffer->GetFormat());
	}
	else if (input_media_track->GetMediaType() == common::MediaType::Audio)
	{
		input_media_track->SetSampleRate(buffer->GetSampleRate());
		input_media_track->GetSample().SetFormat(buffer->GetFormat<common::AudioSample::Format>());
		input_media_track->GetChannel().SetLayout(buffer->GetChannelLayout());
	}


	// 2. due to structural problems I've already made the transcode's context... so, update changed data.
	auto input_transcode_context = _decoders[decoder_id]->GetContext();
	if (buffer->GetMediaType() == common::MediaType::Video)
	{

	}
	else if (buffer->GetMediaType() == common::MediaType::Audio)
	{
		input_transcode_context->SetAudioSampleRate(input_media_track->GetSampleRate());
		input_transcode_context->SetAudioSampleFormat(input_media_track->GetSample().GetFormat());
		input_transcode_context->GetAudioChannel().SetLayout(input_media_track->GetChannel().GetLayout());
	}

	// 3. Get Output Track List. Creates a filter by looking up the encoding context information of the output track.
	auto filter_item = _stage_decoder_to_filter.find(decoder_id);
	if (filter_item == _stage_decoder_to_filter.end())
	{
		logtw("Filter not found");
		return;
	}

	for (auto &filter_id : filter_item->second)
	{
		auto encoder_id = _stage_filter_to_encoder[filter_id];

		if(_encoders.find(encoder_id) == _encoders.end())
		{
			logte("%d track encoder is not allocated", encoder_id);
			continue;
		}

		auto output_transcode_context = _encoders[encoder_id]->GetContext();

		auto transcode_filter = std::make_shared<TranscodeFilter>();

		bool ret = transcode_filter->Configure(input_media_track, input_transcode_context, output_transcode_context);
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

void TranscodeStream::DoFilters(std::shared_ptr<MediaFrame> frame)
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

uint8_t TranscodeStream::NewTrackId(common::MediaType media_type)
{
	uint8_t last_index = 0;

	last_index = _last_track_index;
	_last_track_index++;

	return last_index;
}

common::MediaCodecId TranscodeStream::GetCodecId(ov::String name)
{
	name.MakeUpper();

	// Video codecs
	if (name == "H264")
	{
		return common::MediaCodecId::H264;
	}
	else if (name == "H265")
	{
		return common::MediaCodecId::H265;
	}
	else if (name == "VP8")
	{
		return common::MediaCodecId::Vp8;
	}
	else if (name == "VP9")
	{
		return common::MediaCodecId::Vp9;
	}

	// Audio codecs
	if (name == "AAC")
	{
		return common::MediaCodecId::Aac;
	}
	else if (name == "MP3")
	{
		return common::MediaCodecId::Mp3;
	}
	else if (name == "OPUS")
	{
		return common::MediaCodecId::Opus;
	}

	return common::MediaCodecId::None;
}

bool TranscodeStream::IsVideoCodec(common::MediaCodecId codec_id)
{
	if(codec_id == common::MediaCodecId::H264 || codec_id == common::MediaCodecId::H265 || codec_id == common::MediaCodecId::Vp8 || codec_id == common::MediaCodecId::Vp9)
	{
		return true;
	}

	return false;
}

bool TranscodeStream::IsAudioCodec(common::MediaCodecId codec_id)
{
	if( codec_id == common::MediaCodecId::Aac || codec_id == common::MediaCodecId::Mp3 || codec_id == common::MediaCodecId::Opus)
	{
		return true;
	}

	return false;
}

int TranscodeStream::GetBitrate(ov::String bitrate)
{
	bitrate.MakeUpper();

	int multiplier = 1;

	if (bitrate.HasSuffix("K"))
	{
		multiplier = 1024;
	}
	else if (bitrate.HasSuffix("M"))
	{
		multiplier = 1024 * 1024;
	}

	return static_cast<int>(ov::Converter::ToFloat(bitrate) * multiplier);
}

// Look up the Encode settings by name in the application configuration.
const cfg::Encode *TranscodeStream::GetEncodeByProfileName(const info::Application &application_info, ov::String encode_name)
{
	auto &encodes = application_info.GetConfig().GetEncodeList();

	for (const auto &encode : encodes)
	{
		if (encode.IsActive() == false)
		{
			continue;
		}

		if (encode.GetName() == encode_name)
		{
			// auto &i = encodes.at(0);
			// logte("%p %p %s", &i, &encode, encode.ToString().CStr());
			return &encode;
		}
	}

	return nullptr;
}
