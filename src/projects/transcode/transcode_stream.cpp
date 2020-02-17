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

#define ENABLE_SINGLE_THREAD	1



TranscodeStream::TranscodeStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent)
	: _application_info(application_info)
{
	// Statistical parameters
	_stats_decoded_frame_count = 0;
	_stats_queue_full_count = 0;

	// Determine maximum queue size
	_max_queue_size = 0;

	//store Parent information
	_parent = parent;

	// Store Stream information
	_stream_input = stream;

	// 
	_last_transcode_id = 0;

	// Create Output Stream and Tracks
	if (CreateOutputStream() == 0)
	{
		logte("No output stream generated");
		return;
	}

	if (CreateStageMapping() == 0)
	{
		logte("No stage generated");
		return;
	}

	// Create Docoders
	if (CreateDecoders() == 0)
	{
		logtw("No decoder generated");
	}

	// Create Encoders
	if (CreateEncoders() == 0)
	{
		logtw("No encoder generated");
	}

	// TODO(soulk)
	// I will make and apply a packet drop policy.
	_max_queue_size = 256;

	logti("Transcoder Information. Decoders(%d) Encoders(%d)", _decoders.size(), _encoders.size());

	// Create Data Processing Loop Threads
	try
	{
		_kill_flag = false;

		_thread_looptask = std::thread(&TranscodeStream::LoopTask, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}

	logtd("Started transcode stream thread.");
}

TranscodeStream::~TranscodeStream()
{
	// The thread checked for non-termination and terminated
	if (_kill_flag != true)
	{
		Stop();
	}
}

void TranscodeStream::Stop()
{
	_kill_flag = true;

	logtd("Wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag ? "true" : "false");

	_queue_input_packets.abort();
	_queue_decoded_frames.abort();
	_queue_filterd_frames.abort();

	_queue_event.Notify();

	if (_thread_looptask.joinable())
	{
		_thread_looptask.join();
	}

}

bool TranscodeStream::Push(std::shared_ptr<MediaPacket> packet)
{
	if(_max_queue_size == 0)
	{
		// Drop packets because the transcoding information has not been generated.
		return true;
	}

	if (_queue_input_packets.size() > _max_queue_size)
	{
		logti("Queue(stream) is full, please check your system: (queue: %zu > limit: %llu)", _queue_input_packets.size(), _max_queue_size);
		return false;
	}

	_queue_input_packets.push(std::move(packet));

	_queue_event.Notify();

	return true;
}


// Create Output Stream and Encoding Transcode Context
int32_t TranscodeStream::CreateOutputStream()
{
	int32_t created_stream_count = 0;


	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. Create new stream and new track
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Get [application->Streams] list of application configuration.
	auto &cfg_streams = _application_info.GetConfig().GetStreamList();

	for (const auto &cfg_stream : cfg_streams)
	{
		// Validation check.
		if ( cfg_stream.GetName().GetLength() == 0)
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
			// auto  input_track_id = input_track_item.first;
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
					logte("can't find encode configuration. name(%s)", cfg_profile.GetName().CStr());
					continue;
				}

				if (input_track_media_type != common::MediaType::Video && input_track_media_type != common::MediaType::Audio)
				{
					logte("Unsupported media type of input track. type(%d)", input_track_media_type);
					continue;
				}				

				auto new_outupt_track = std::make_shared<MediaTrack>();

				if (input_track_media_type == common::MediaType::Video)
				{
					auto cfg_encode_video = cfg_encode->GetVideoProfile();

					if ((cfg_encode_video != nullptr) && (cfg_encode_video->IsActive()) )
					{
						new_outupt_track->SetBypass( cfg_encode_video->IsBypass() );
						new_outupt_track->SetId( NewTrackId(new_outupt_track->GetMediaType()) );
						new_outupt_track->SetMediaType(common::MediaType::Video);

						// When bypass is enabled, it gets information from tracks in the input stream.
						if( new_outupt_track->IsBypass() == true)
						{
							new_outupt_track->SetCodecId( input_track->GetCodecId() );
							new_outupt_track->SetBitrate( input_track->GetBitrate() );
							new_outupt_track->SetWidth( input_track->GetWidth() );
							new_outupt_track->SetHeight( input_track->GetHeight() );
							new_outupt_track->SetFrameRate( input_track->GetFrameRate());
							new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							new_outupt_track->SetCodecId( GetCodecId(cfg_encode_video->GetCodec()) );
							new_outupt_track->SetBitrate( GetBitrate(cfg_encode_video->GetBitrate()) );
							new_outupt_track->SetWidth(cfg_encode_video->GetWidth());
							new_outupt_track->SetHeight(cfg_encode_video->GetHeight());
							new_outupt_track->SetFrameRate(cfg_encode_video->GetFramerate());

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

					if ((cfg_encode_audio != nullptr) && (cfg_encode_audio->IsActive()) )
					{
						new_outupt_track->SetId( NewTrackId(new_outupt_track->GetMediaType()) );
						new_outupt_track->SetMediaType(common::MediaType::Audio);
						new_outupt_track->SetBypass( cfg_encode_audio->IsBypass() );

						if( new_outupt_track->IsBypass() == true)
						{
							new_outupt_track->SetCodecId( input_track->GetCodecId()  );
							new_outupt_track->SetBitrate( input_track->GetBitrate()  );
							new_outupt_track->SetSampleRate(input_track->GetSampleRate() );
							new_outupt_track->GetChannel().SetLayout( input_track->GetChannel().GetLayout() );
							new_outupt_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
							new_outupt_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							new_outupt_track->SetCodecId( GetCodecId(cfg_encode_audio->GetCodec()) );
							new_outupt_track->SetBitrate( GetBitrate(cfg_encode_audio->GetBitrate()) );
							new_outupt_track->SetSampleRate(cfg_encode_audio->GetSamplerate());
							new_outupt_track->GetChannel().SetLayout(cfg_encode_audio->GetChannel() == 1 ? common::AudioChannel::Layout::LayoutMono : common::AudioChannel::Layout::LayoutStereo);
							new_outupt_track->SetTimeBase(1, cfg_encode_audio->GetSamplerate());

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
	
	ov::String temp_debug_msg = "\r\n======= Stage Map of Transcoder =======\n";
	temp_debug_msg.AppendFormat(" - app(%s/%d), stream(%s/%d)\n"
		, _application_info.GetName().CStr()
		, _application_info.GetId()
		, _stream_input->GetName().CStr()
		, _stream_input->GetId());

	for (auto &iter : _map_stage_context)
	{
		auto key_pair = iter.first;
		auto &flow_context = iter.second;
		auto encode_profile_name = key_pair.first;
		auto encode_media_type = key_pair.second;

		for (auto &iter_output_tracks : flow_context->_output_tracks)
		{
			auto stream = iter_output_tracks.first;
			auto track_info =  iter_output_tracks.second;

			auto input_id = flow_context->_input_track->GetId();
			auto decoder_id = flow_context->_input_track->GetId();
			auto filter_id = flow_context->_transcoder_id;
			auto encoder_id = flow_context->_transcoder_id;
			auto output_id = track_info->GetId();

			if (track_info->IsBypass() == true)
			{
				_stage_input_to_output[input_id].push_back ( make_pair(stream, output_id) );
			}
			else
			{
				// Map of InputTrack -> Decoder 
				_stage_input_to_decoder[input_id] = decoder_id;
				
				// Map of Decoder -> Filters
				if(_stage_decoder_to_filter.find(decoder_id) == _stage_decoder_to_filter.end())
				{
					_stage_decoder_to_filter[decoder_id].push_back( filter_id );
				}
				else
				{
					auto filter_ids = _stage_decoder_to_filter[decoder_id];
					bool found = false;
					for(auto it_filter_id : filter_ids)
					{
						if(it_filter_id == filter_id)
						{
							found = true;
						}
					}

					if( found == false )
					{
						_stage_decoder_to_filter[decoder_id].push_back( filter_id );
					}
				}

				// Map of Filter -> Encoder 
				_stage_filter_to_encoder[filter_id] = encoder_id;
				
				// Map of Encoder -> OutputTrack 
				_stage_encoder_to_output[encoder_id].push_back ( make_pair(stream, output_id ) );
			}
		}

		// for debug log
		ov::String temp_str = "";
		for (auto &iter_output_tracks : flow_context->_output_tracks)
		{
			auto stream = iter_output_tracks.first;
			auto track_info =  iter_output_tracks.second;
			
			temp_str.AppendFormat("[%d:%s]", track_info->GetId(), (track_info->IsBypass()) ? "Pss" : "Enc");
		}

		// for debug log
		temp_debug_msg.AppendFormat(" - Encode Profile(%s:%s) / Flow InputTrack[%d] => Decoder[%d] => Filter[%d] => Encoder[%d] => OutputTraks%s\n"
			  , encode_profile_name.CStr(), (encode_media_type == common::MediaType::Video) ? "Video" : "Audio"
		      , flow_context->_input_track->GetId()
		      , flow_context->_input_track->GetId()
		      , flow_context->_transcoder_id
		      , flow_context->_transcoder_id
		      , temp_str.CStr());

		created_stage_map++;

	}
	logtd(temp_debug_msg);


	return created_stage_map;

}

// 인코더 프로파일 중 실제로 사용되는 정보를 저장한다. 이 정보는 인코더 중복 생성 방지 및 스테이지 별 트랙아이디 매핑을 위한 목적으로 사용횐다.
void TranscodeStream::StoreStageContext(ov::String encode_profile_name, common::MediaType media_type,  std::shared_ptr<MediaTrack> input_track, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track)
{
	auto key = std::make_pair(encode_profile_name, media_type);

	auto encoder_context_object = _map_stage_context.find( key );
	if (encoder_context_object == _map_stage_context.end())
	{
		auto obj = std::make_shared<TranscodeStageContext>();
		obj->_transcoder_id = _last_transcode_id++;
		obj->_input_track = input_track;
		obj->_output_tracks.push_back( std::make_pair(output_stream, output_track) );

		_map_stage_context [ key ] = obj;
	}
	else {
		auto obj = encoder_context_object->second;

		obj->_output_tracks.push_back( std::make_pair(output_stream, output_track) );
	}
}


// Create Docoders
//
//  Input Stream->Tracks 정보를 참고하여 디코더를 생성한다
//  - Create docder by the number of tracks
// _decoders[track_id] + (transcode_context)

int32_t TranscodeStream::CreateDecoders()
{
	int32_t created_decoder_count = 0;

#if 1
	for (auto iter : _stage_input_to_decoder)
	{
		auto input_track_id = iter.first;
		auto decoder_track_id = iter.second;

		auto track_item = _stream_input->GetTracks().find(input_track_id);
		if(track_item == _stream_input->GetTracks().end())
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
#else	
	for (auto &track_item : _stream_input->GetTracks())
	{
		auto &track = track_item.second;
		std::shared_ptr<TranscodeContext> input_context = nullptr;

		OV_ASSERT2(track != nullptr);

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

		CreateDecoder(track->GetId(), input_context);

		created_decoder_count++;
	}
#endif
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
	auto decoder = std::move(TranscodeDecoder::CreateDecoder(track->GetCodecId(), input_context));
	if (decoder == nullptr)
	{
		logte("Decoder allocation failed");
		return false;
	}

	_decoders[decoder_track_id] = std::move(decoder);

	return true;
}

// 입력 트랙 존재 여부에 따라서 인코더를 생성해야 한다.
int32_t TranscodeStream::CreateEncoders()
{
	int32_t created_encoder_count = 0;

	for (auto &iter : _map_stage_context)
	{
		// auto key_pair = iter.first;
		// auto encode_profile_name = key_pair.first;
		// auto encode_media_type = key_pair.second;

		auto &flow_context = iter.second;
		auto encoder_track_id = flow_context->_transcoder_id;

		// 인코더에서 아웃풋 출력이 있는 트랙에 대해서 인코더를 생성한다
		auto stage_items = _stage_encoder_to_output.find(encoder_track_id);
		if(stage_items == _stage_encoder_to_output.end())
		{
			continue;
		}

		if(stage_items->second.size() == 0)
		{
			logtw("Encoder is not required.");
			continue;
		}

		// 출력 트랙들 중 첫번째 트랙의 정보를 가져온다. 
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
			// TODO(soulk): 추가 파라미터 설정
			//	 - 프로파일 레벨
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
			// TODO(soulk): 추가 파라미터 설정
			//   - 채널 레이아웃
			//	 - 프로파일 레벨
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
		logte("Encoder allication failed");
		return false;
	}

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
	// 바이패스 처리 
	auto stage_item_to_output = _stage_input_to_output.find(track_id);
	if (stage_item_to_output != _stage_input_to_output.end())
	{
		auto &output_tracks = stage_item_to_output->second;

		for(auto iter : output_tracks)
		{
			auto output_stream = iter.first;
			auto output_track_id = iter.second;

			auto clone_packet = packet->ClonePacket();

			clone_packet->SetTrackId(output_track_id);

			SendFrame(output_stream, std::move(clone_packet));
		}
	}


	// 디코더 처리
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

	logtp("[#%d] Trying to decode a frame (PTS: %lld)", track_id, packet->GetPts());
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

			// TODO(soulk): Re-create the filter context
			// TODO(soulk): Re-create the encoder context

			decoded_frame->SetTrackId(decoder_id);
			ChangeOutputFormat(decoded_frame.get());

			// It is intended that there is no "break;" statement here

		case TranscodeResult::DataReady:
			decoded_frame->SetTrackId(decoder_id);

			logtp("[#%d] A packet is decoded (PTS: %lld)", decoder_id, decoded_frame->GetPts());

			_stats_decoded_frame_count++;

			if (_queue_decoded_frames.size() > _max_queue_size)
			{
				logti("Decoded frame queue is full, please check your system");
				return result;
			}

			_queue_decoded_frames.push(std::move(decoded_frame));

			_queue_event.Notify();

			break;

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

	logtp("[#%d] Trying to apply a filter to the frame (PTS: %lld)", track_id, decoded_frame->GetPts());
	filter->SendBuffer(std::move(decoded_frame));

	while (true)
	{
		TranscodeResult result;
		auto filtered_frame = filter->RecvBuffer(&result);

		if (static_cast<int>(result) < 0)
		{
			return result;
		}

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		switch (result)
		{
		case TranscodeResult::DataReady:
			filtered_frame->SetTrackId(track_id);

			logtp("[#%d] A frame is filtered (PTS: %lld)", track_id, filtered_frame->GetPts());

			// if (_queue_filterd_frames.size() > _max_queue_size)
			// {
			// 	logti("Filtered frame queue is full, please decrease encoding options (resolution, bitrate, framerate)");

			// 	return result;
			// }

			_queue_filterd_frames.push(std::move(filtered_frame));

			_queue_event.Notify();

			break;

		default:
			return result;
		}
	}
}

TranscodeResult TranscodeStream::EncodeFrame(int32_t filter_id, std::shared_ptr<const MediaFrame> frame)
{
	// 인코더 아이디 조회
	auto encoder_id = _stage_filter_to_encoder[filter_id];

	auto encoder_item = _encoders.find(encoder_id);
	if (encoder_item == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_item->second.get();

	logtp("[#%d] Trying to encode the frame (PTS: %lld)", encoder_id, frame->GetPts());

	encoder->SendBuffer(std::move(frame));

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
			// logtd("[#%d] A packet is encoded (PTS: %lld)", encoder_id, encoded_packet->GetPts());

			// 인코딩된 패킷을 전송할 출력 트랙이 존재하는지 탐색
			auto stage_item = _stage_encoder_to_output.find(encoder_id);
			if(stage_item == _stage_encoder_to_output.end())
			{
				continue;
			}

			// 출력할 트랙이 존재한다면, 인코딩된 패킷을 복사하여 해당 트랙으로 전송한다.
			for( auto &iter : stage_item->second )
			{
				auto &output_stream = iter.first;
				auto output_track_id = iter.second;

				auto clone_packet = encoded_packet->ClonePacket();
				clone_packet->SetTrackId(output_track_id);

				// if(encoder_id == 1 || encoder_id == 2)
				// 	logtd("  [#%d -> %d] A packet is encoded (PTS: %lld)", encoder_id, output_track_id, clone_packet->GetPts());

				// Send the packet to MediaRouter
				SendFrame(output_stream, std::move(clone_packet));
			}
		}
	}
}

// 디코딩 & 인코딩 스레드
void TranscodeStream::LoopTask()
{
	logtd("Started transcode stream decode thread");

	CreateStreams();

	time_t base_time;
	time(&base_time);

	while (!_kill_flag)
	{
		_queue_event.Wait();

		time_t curr_time;
		time(&curr_time);

		// for statistics 
		if(difftime(curr_time, base_time) >= 30)
		{
			logti("stats of stream[%s/%s], decode.ready[%d], filter.ready[%d], encode.ready[%d]"
				,_application_info.GetName().CStr()
				,_stream_input->GetName().CStr()
				,_queue_input_packets.size()
				,_queue_decoded_frames.size()
				,_queue_filterd_frames.size());

			base_time = curr_time;
		}


		if (_queue_input_packets.size() > 0)
		{
			auto packet = _queue_input_packets.pop_unique();
			if (packet != nullptr)
			{
				int32_t track_id = packet->GetTrackId();

				DecodePacket(track_id, std::move(packet));
			}
		}

		if (_queue_decoded_frames.size() > 0)
		{
			auto frame = _queue_decoded_frames.pop_unique();
			if (frame != nullptr)
			{
				DoFilters(std::move(frame));
			}
		}


		while((_queue_filterd_frames.size() > 0) && (_queue_filterd_frames.IsAborted() == false))
		{
			auto frame = _queue_filterd_frames.pop_unique();
			if (frame != nullptr)
			{
				int32_t filter_id = frame->GetTrackId();

				EncodeFrame(filter_id, std::move(frame));
			}
		}

		// TODO(soulk) Packet이 존재하는 경우에만 Loop를 처리할 수 있는 방법은 없나?
		// usleep(1);
	}

	// 스트림 삭제 전송
	DeleteStreams();

	logtd("Terminated transcode stream decode thread");
}

void TranscodeStream::CreateStreams()
{
	for (auto &iter : _stream_outputs)
	{
		_parent->CreateStream(iter.second);
	}
}

void TranscodeStream::DeleteStreams()
{
	for (auto &iter : _stream_outputs)
	{
		_parent->DeleteStream(iter.second);
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


	// 2. due to structural problems I've already made the encoder's context... I checked and corrected it.
	auto input_transcode_context = _decoders[decoder_id]->GetContext();
	if (buffer->GetMediaType() == common::MediaType::Video)
	{

	}
	else if (buffer->GetMediaType() == common::MediaType::Audio)
	{
		input_transcode_context->SetAudioSampleFormat(buffer->GetFormat<common::AudioSample::Format>());
		input_transcode_context->GetAudioChannel().SetLayout(buffer->GetChannelLayout());
	}

	// 3. Get Output Track List. Creates a filter by looking up the encoding context information of the output track.
	auto filter_item = _stage_decoder_to_filter.find(decoder_id);
	if(filter_item == _stage_decoder_to_filter.end())
	{
		logtw("No filter list found");
		return;
	}

	for(auto &filter_id : filter_item->second)
	{
		auto encoder_id = _stage_filter_to_encoder[filter_id];

		auto output_transcode_context = _encoders[encoder_id]->GetContext();

		auto transcode_filter = std::make_shared<TranscodeFilter>();

		bool ret = transcode_filter->Configure(input_media_track, input_transcode_context, output_transcode_context);
		if(ret == true)
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
	if(filter_item == _stage_decoder_to_filter.end())
	{
		logtw("No filter list found");
		return;
	}

	for(auto &filter_id : filter_item->second)
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
	else if (name == "VP8")
	{
		return common::MediaCodecId::Vp8;
	}
	else if (name == "VP9")
	{
		return common::MediaCodecId::Vp9;
	}

	// Audio codecs
	if (name == "FLV")
	{
		return common::MediaCodecId::Flv;
	}
	else if (name == "AAC")
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
const cfg::Encode* TranscodeStream::GetEncodeByProfileName(const info::Application &application_info, ov::String encode_name)
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

