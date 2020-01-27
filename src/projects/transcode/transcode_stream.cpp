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



TranscodeStream::TranscodeStream(const info::Application &application_info, const std::shared_ptr<StreamInfo> &stream_info, TranscodeApplication *parent)
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
	_stream_info_input = stream_info;

	// Create Output Stream and Tracks
	if (CreateOutputStream() == 0)
	{
		logte("No output stream generated");
		return;
	}

	// Create Docoders
	if (CreateDecoders() == 0)
	{
		logte("No decoder generated");
		return;
	}

	// Create Encoders
	if (CreateEncoders() == 0)
	{
		logte("No encoder generated");
		return;
	}

	// _max_queue_size : 255
	_max_queue_size = (_encoders.size() > 0x0F) ? 0xFF : _encoders.size() * 256;

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
	if (_queue_input_packets.size() > _max_queue_size)
	{
		logti("Queue(stream) is full, please check your system: (queue: %zu > limit: %llu)", _queue_input_packets.size(), _max_queue_size);
		return false;
	}

	_queue_input_packets.push(std::move(packet));

	_queue_event.Notify();

	return true;
}

// Create Docoders
//
//  Input StreamInfo->Tracks 정보를 참고하여 디코더를 생성한다
//  - Create docder by the number of tracks
// _decoders[track_id] + (transcode_context)

int32_t TranscodeStream::CreateDecoders()
{
	int32_t created_decoder_count = 0;

	for (auto &track_item : _stream_info_input->GetTracks())
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

	return created_decoder_count;
}

bool TranscodeStream::CreateDecoder(int32_t media_track_id, std::shared_ptr<TranscodeContext> input_context)
{
	// input_context must be decoding context
	OV_ASSERT2(input_context != nullptr);
	OV_ASSERT2(input_context->IsEncodingContext() == false);

	auto track = _stream_info_input->GetTrack(media_track_id);
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

	_decoders[media_track_id] = std::move(decoder);

	return true;
}


// Create Output Stream and Encoding Transcode Context
int32_t TranscodeStream::CreateOutputStream()
{
	int32_t created_stream_count = 0;


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

		auto stream_info_output = std::make_shared<StreamInfo>(StreamSourceType::LIVE_TRANSCODER);

		// Create a new stream name.
		auto stream_name = cfg_stream.GetName();
		if (::strstr(stream_name.CStr(), "${OriginStreamName}") != nullptr)
		{
			stream_name = stream_name.Replace("${OriginStreamName}", _stream_info_input->GetName());
		}
		stream_info_output->SetName(stream_name);


		// Look up all tracks in the input stream.
		for (auto &input_track_item : _stream_info_input->GetTracks())
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

				if (input_track_media_type == common::MediaType::Video)
				{
					auto cfg_encode_video = cfg_encode->GetVideoProfile();

					if ((cfg_encode_video != nullptr) && (cfg_encode_video->IsActive()) )
					{
						auto new_track = std::make_shared<MediaTrack>();

						new_track->SetBypass( cfg_encode_video->IsBypass() );
						new_track->SetId( NewTrackId(new_track->GetMediaType()) );
						new_track->SetMediaType(common::MediaType::Video);

						// When bypass is enabled, it gets information from tracks in the input stream.
						if( new_track->IsBypass() == true)
						{
							new_track->SetCodecId( input_track->GetCodecId() );
							new_track->SetBitrate( input_track->GetBitrate() );
							new_track->SetWidth( input_track->GetWidth() );
							new_track->SetHeight( input_track->GetHeight() );
							new_track->SetFrameRate( input_track->GetFrameRate());
							new_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							new_track->SetCodecId( GetCodecId(cfg_encode_video->GetCodec()) );
							new_track->SetBitrate( GetBitrate(cfg_encode_video->GetBitrate()) );
							new_track->SetWidth(cfg_encode_video->GetWidth());
							new_track->SetHeight(cfg_encode_video->GetHeight());
							new_track->SetFrameRate(cfg_encode_video->GetFramerate());

							// The timebase format will change by the decoder event.
							new_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());

						}

						logti("mapping to video track [%s][%d] => [%s][%d]", _stream_info_input->GetName().CStr(), input_track->GetId(), stream_name.CStr(), new_track->GetId());

						stream_info_output->AddTrack(new_track);

						_tracks_map[input_track->GetId()].push_back( make_pair(new_track->GetId(), new_track) );
					}
				}

				if (input_track_media_type == common::MediaType::Audio)
				{
					auto cfg_encode_audio = cfg_encode->GetAudioProfile();

					if ((cfg_encode_audio != nullptr) && (cfg_encode_audio->IsActive()) )
					{
						auto new_track = std::make_shared<MediaTrack>();

						new_track->SetId( NewTrackId(new_track->GetMediaType()) );
						new_track->SetMediaType(common::MediaType::Audio);
						new_track->SetBypass( cfg_encode_audio->IsBypass() );

						if( new_track->IsBypass() == true)
						{
							new_track->SetCodecId( input_track->GetCodecId()  );
							new_track->SetBitrate( input_track->GetBitrate()  );
							new_track->SetSampleRate(input_track->GetSampleRate() );
							new_track->GetChannel().SetLayout( input_track->GetChannel().GetLayout() );
							new_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
							new_track->SetTimeBase(input_track->GetTimeBase().GetNum(), input_track->GetTimeBase().GetDen());
						}
						else
						{
							new_track->SetCodecId( GetCodecId(cfg_encode_audio->GetCodec()) );
							new_track->SetBitrate( GetBitrate(cfg_encode_audio->GetBitrate()) );
							new_track->SetSampleRate(cfg_encode_audio->GetSamplerate());
							new_track->GetChannel().SetLayout(cfg_encode_audio->GetChannel() == 1 ? common::AudioChannel::Layout::LayoutMono : common::AudioChannel::Layout::LayoutStereo);

							// The timebase, sample format will change by the decoder event.
							new_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
							new_track->SetTimeBase(1, cfg_encode_audio->GetSamplerate());
						}

						logti("mapping to audio track [%s][%d] => [%s][%d]", _stream_info_input->GetName().CStr(), input_track->GetId(), stream_name.CStr(), new_track->GetId());

						stream_info_output->AddTrack(new_track);

						_tracks_map[input_track->GetId()].push_back( make_pair(new_track->GetId(), new_track) );
					}
				}
			}
		}

		// Add to Output Stream List. The key is the output stream name.
		_stream_info_outputs.insert(std::make_pair(stream_name, stream_info_output));

		created_stream_count++;
	}

	return created_stream_count;
}


// 입력 트랙 존재 여부에 따라서 인코더를 생성해야 한다.
int32_t TranscodeStream::CreateEncoders()
{
	int32_t created_encoder_count = 0;

	for (auto &iter : _stream_info_outputs)
	{
		// auto &output_stream_name = iter.first;
		auto &output_stream_info = iter.second;

		for (auto &media_track : output_stream_info->GetTracks())
		{
			// auto &track_id = media_track.first;
			auto &track = media_track.second;
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

				CreateEncoder(track, new_output_transcode_context);
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

				CreateEncoder(track, new_output_transcode_context);
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
	}

	return created_encoder_count;
}


bool TranscodeStream::CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context)
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

	_encoders[media_track->GetId()] = std::move(encoder);

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
	// If the output track is a packet equivalent to BYPASS, it will be forwarded directly to the media route without any transcoding process.
	bool needed_transcode = false;
	// GetByassTrackInfo(track_id, bypass_cnt, nbypass_cnt);

	auto output_track_ids = _tracks_map.find(track_id);
	if (output_track_ids == _tracks_map.end())
	{
		return TranscodeResult::NoData;
	}

	auto pair_mapped_tracks = output_track_ids->second;
	for ( auto pair_item : pair_mapped_tracks )
	{
		int32_t output_track_id = pair_item.first;
		auto output_track = pair_item.second;

		if(output_track->IsBypass() == true)
		{
			auto clone_packet = packet->ClonePacket();

			clone_packet->SetTrackId(output_track_id);
			SendFrame(std::move(clone_packet));
		}
		else
		{
			needed_transcode = true;
		}
	}


	// If there are no tracks that require transcoding, exit.
	if(needed_transcode == false)
	{
		return TranscodeResult::NoData;
	}

	auto decoder_item = _decoders.find(track_id);
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

			decoded_frame->SetTrackId(track_id);
			ChangeOutputFormat(decoded_frame.get());

			// It is intended that there is no "break;" statement here

		case TranscodeResult::DataReady:
			decoded_frame->SetTrackId(track_id);

			logtp("[#%d] A packet is decoded (PTS: %lld)", track_id, decoded_frame->GetPts());

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

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		switch (result)
		{
		case TranscodeResult::DataReady:
			filtered_frame->SetTrackId(track_id);

			// logtd("[#%d] A frame is filtered (PTS: %lld)", track_id, filtered_frame->GetPts());

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

TranscodeResult TranscodeStream::EncodeFrame(int32_t track_id, std::shared_ptr<const MediaFrame> frame)
{
	auto encoder_item = _encoders.find(track_id);
	if (encoder_item == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	auto encoder = encoder_item->second.get();

	logtp("[#%d] Trying to encode the frame (PTS: %lld)", track_id, frame->GetPts());

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
			encoded_packet->SetTrackId(track_id);

			logtp("[#%d] A packet is encoded (PTS: %lld)", track_id, encoded_packet->GetPts());

			// Send the packet to MediaRouter
			SendFrame(std::move(encoded_packet));
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

		if(difftime(curr_time, base_time) >= 1)
		{
			logtd("stats stream[%s/%s], decode.ready[%d], filter.ready[%d], encode.ready[%d]"
				,_application_info.GetName().CStr()
				,_stream_info_input->GetName().CStr()
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


		if(_queue_filterd_frames.size() > 0)
		{
			auto frame = _queue_filterd_frames.pop_unique();
			if (frame != nullptr)
			{
				int32_t track_id = frame->GetTrackId();

				EncodeFrame(track_id, std::move(frame));
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
	for (auto &iter : _stream_info_outputs)
	{
		_parent->CreateStream(iter.second);
	}
}

void TranscodeStream::DeleteStreams()
{
	for (auto &iter : _stream_info_outputs)
	{
		_parent->DeleteStream(iter.second);
	}

	_stream_info_outputs.clear();
}

void TranscodeStream::SendFrame(std::shared_ptr<MediaPacket> packet)
{
	// uint8_t track_id = static_cast<uint8_t>(packet->GetTrackId());

	// TODO(soulk): Performance improvement is needed!!! It's urgent!!!
	for (auto &iter : _stream_info_outputs)
	{
		auto &output_stream_info = iter.second;

		for (auto &media_track : output_stream_info->GetTracks())
		{
			if (packet->GetTrackId() == media_track.first)
			{
				auto clone_packet = packet->ClonePacket();

				// logtd("[#%d] Trying to outgoing the packet (%s, PTS: %lld)", clone_packet->GetTrackId(), output_stream_info->GetName().CStr(), clone_packet->GetPts() * 1000 / (int64_t)media_track.second->GetTimeBase().GetDen());

				_parent->SendFrame(output_stream_info, std::move(clone_packet));
			}
		}
	}
}


void TranscodeStream::CreateFilters(MediaFrame *buffer)
{
	// 1. Update information for input media track from decoded media frame.
	auto &input_media_track = _stream_info_input->GetTrack(buffer->GetTrackId());
	if (input_media_track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", buffer->GetTrackId());

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
	auto input_transcode_context = _decoders[buffer->GetTrackId()]->GetContext();
	if (buffer->GetMediaType() == common::MediaType::Video)
	{

	}
	else if (buffer->GetMediaType() == common::MediaType::Audio)
	{
		input_transcode_context->SetAudioSampleFormat(buffer->GetFormat<common::AudioSample::Format>());
		input_transcode_context->GetAudioChannel().SetLayout(buffer->GetChannelLayout());
	}


	// 3. Get Output Track List. Creates a filter by looking up the encoding context information of the output track.
	int32_t track_id = buffer->GetTrackId();
	auto output_track_ids = _tracks_map.find(track_id);
	if (output_track_ids == _tracks_map.end())
	{
		return;
	}

	auto pair_mapped_tracks = output_track_ids->second;
	for ( auto pair_item : pair_mapped_tracks )
	{
		int32_t output_track_id = pair_item.first;

		auto output_transcode_context = _encoders[output_track_id]->GetContext();

		_filters[output_track_id] = std::make_shared<TranscodeFilter>(input_media_track, input_transcode_context, output_transcode_context);
	}
}

void TranscodeStream::DoFilters(std::shared_ptr<MediaFrame> frame)
{

	// 패킷의 트랙 아이디를 조회
	int32_t track_id = frame->GetTrackId();

	auto output_track_ids = _tracks_map.find(track_id);
	if (output_track_ids == _tracks_map.end())
	{
		return;
	}

	auto pair_mapped_tracks = output_track_ids->second;
	for ( auto pair_item : pair_mapped_tracks )
	{
		int32_t output_track_id = pair_item.first;
		auto output_track = pair_item.second;

		// 디코딩된 프레임의 대상 트랙이 Bybass 이면, 필터에 전송하지 않는다.
		if(output_track->IsBypass() == true)
		{
			continue;
		}

		auto frame_clone = frame->CloneFrame();
		if (frame_clone == nullptr)
		{
			logte("FilterTask -> Unknown Frame");
			continue;
		}

		// if(output_track_id == 1)
			// logtd("[#%d -> #%d] Trying to filter a frame (PTS: %lld)", track_id, output_track_id,  frame->GetPts());
		
		FilterFrame(output_track_id, std::move(frame_clone));
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

