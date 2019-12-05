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
#include <base/common_types.h>

#define OV_LOG_TAG "TranscodeStream"

std::map<uint32_t, std::set<ov::String>> TranscodeStream::_stream_list;

common::MediaCodecId GetCodecId(ov::String name)
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

int GetBitrate(ov::String bitrate)
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

TranscodeStream::TranscodeStream(const info::Application *application_info, std::shared_ptr<StreamInfo> stream_info, TranscodeApplication *parent)
	: _application_info(application_info)
{
	logtd("Transcode stream is created: %s", stream_info->GetName().CStr());

	// 통계 정보 초기화
	_stats_decoded_frame_count = 0;

	// maximum queue size
	_max_queue_size = 0;

	_stats_queue_full_count = 0;

	// 부모 클래스
	_parent = parent;

	// 입력 스트림 정보
	_stream_info_input = stream_info;

	_stream_list[_application_info->GetId()];

	// Generate track list by profile(=encode name)
	auto encodes = _application_info->GetEncodes();
	std::map<ov::String, std::vector<uint8_t>> profile_tracks;
	std::vector<uint8_t> tracks;
	std::vector<std::pair<uint8_t, std::shared_ptr<MediaTrack>>> bypass_tracks;

	// Build a list of all input audio/video tracks so that 
	const auto &stream_tracks = _stream_info_input->GetTracks();
	std::vector<std::shared_ptr<MediaTrack>> input_video_tracks, input_audio_tracks, transcoded_tracks;
	for (const auto &stream_track : stream_tracks)
	{
		if (stream_track.second->GetMediaType() == common::MediaType::Video)
		{
			input_video_tracks.emplace_back(stream_track.second);
		}
		else if (stream_track.second->GetMediaType() == common::MediaType::Audio)
		{
			input_audio_tracks.emplace_back(stream_track.second);
		}
	}

	for(const auto &encode : encodes)
	{
		if (encode.IsActive() == false)
		{
			continue;
		}

		const auto &video_profiles = encode.GetVideoProfiles();
		
		auto profile_name = encode.GetName();
		auto input_video_track_iterator = input_video_tracks.begin();

		for (const auto &video_profile : video_profiles)
		{
			if (input_video_track_iterator == input_video_tracks.end())
			{
				logtw("Encode %s has more active video tracks specified than the input stream provides (%zu)", profile_name.CStr(), input_video_tracks.size());
				break;
			}

			if(video_profile.IsActive())
			{
				if (*input_video_track_iterator != nullptr)
				{
					if (video_profile.IsBypass())
					{
						if (*input_video_track_iterator)
						{
							uint8_t track_id = GetTrackId(common::MediaType::Video);
							if (track_id)
							{
								_bypass_routes.emplace((*input_video_track_iterator)->GetId(), track_id);
								bypass_tracks.emplace_back(track_id, *input_video_track_iterator);
								tracks.push_back(track_id);
							}
						}
					}
					else
					{
						auto context = std::make_shared<TranscodeContext>(
							true,
							GetCodecId(video_profile.GetCodec()),
							GetBitrate(video_profile.GetBitrate()),
							video_profile.GetWidth(), video_profile.GetHeight(),
							video_profile.GetFramerate()
						);
						uint8_t track_id = AddOutputContext(common::MediaType::Video, context);
						if(track_id)
						{
							tracks.push_back(track_id);
							transcoded_tracks.emplace_back(*input_video_track_iterator);
						}
					}
				}
				else
				{
					logte("Empty input video track provided");
				}
			}

			// This is put here intentionally - by having the iterator here and not inside the previous block
			// allows to "skip" some streams in case of multiple video tracks from a single provider by adding inactive
			// video encodes
			++input_video_track_iterator;
		}

		const auto &audio_profiles = encode.GetAudioProfiles();
		auto input_audio_track_iterator = input_audio_tracks.begin();
	
		for (const auto &audio_profile : audio_profiles)
		{
			if (input_audio_track_iterator == input_audio_tracks.end())
			{
				logtw("Encode %s has more active audio tracks specified than the input stream provides (%zu)", profile_name.CStr(), input_audio_tracks.size());
				continue;
			}

			if(audio_profile.IsActive())
			{
				if (*input_video_track_iterator != nullptr)
				{
					if (audio_profile.IsBypass())
					{
						if (*input_audio_track_iterator)
						{
							uint8_t track_id = GetTrackId(common::MediaType::Audio);
							if (track_id)
							{
								_bypass_routes.emplace((*input_audio_track_iterator)->GetId(), track_id);
								bypass_tracks.emplace_back(track_id, *input_audio_track_iterator);
								tracks.push_back(track_id);
							}
						}
					}
					else
					{
						auto context = std::make_shared<TranscodeContext>(
							true,
							GetCodecId(audio_profile.GetCodec()),
							GetBitrate(audio_profile.GetBitrate()),
							audio_profile.GetSamplerate()
						);
						uint8_t track_id = AddOutputContext(common::MediaType::Audio, context);
						if(track_id)
						{
							tracks.push_back(track_id);
							transcoded_tracks.emplace_back(*input_audio_track_iterator);
						}
					}
				}
				else
				{
					logte("Empty input audio track provided");
				}
			}

			// This is put here intentionally - by having the iterator here and not inside the previous block
			// allows to "skip" some tracks in case of multiple audio tracks from a single provider by adding inactive
			// audio encodes
			++input_audio_track_iterator;
		}

		if (!tracks.empty())
		{
			profile_tracks[profile_name] = tracks;
			tracks.clear();
		}
	}

	// Prepare decoders if needed
	for(auto &track : transcoded_tracks)
	{
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
	}

	if(_decoders.empty() && bypass_tracks.empty())
	{
		_output_contexts.clear();
		return;
	}

	// Generate track list by stream
	auto streams = _application_info->GetStreamList();

	for (const auto &stream : streams)
	{
		auto stream_name = stream.GetName();
		if (::strstr(stream_name.CStr(), "${OriginStreamName}") == nullptr)
		{
			logtw("Current stream setting (%s) does not use ${OriginStreamName} macro", stream_name.CStr());
		}

		stream_name = stream_name.Replace("${OriginStreamName}", stream_info->GetName());

		if (!AddStreamInfoOutput(stream_name))
		{
			continue;
		}

		auto profiles = stream.GetProfiles();

		for (const auto &profile : profiles)
		{
			auto item = profile_tracks.find(profile.GetName());
			if (item == profile_tracks.end())
			{
				logtw("Encoder for [%s] does not exist in Server.xml", profile.GetName().CStr());
				continue;
			}
			tracks = item->second;
		}
		_stream_tracks[stream_name] = tracks;
		tracks.clear();
	}

	// Generate streams for bypassed tracks
	for(const auto &bypass_track : bypass_tracks)
	{
		for(auto stream_track : _stream_tracks)
		{
			auto it = find(stream_track.second.begin(), stream_track.second.end(), bypass_track.first);
			if(it == stream_track.second.end())
			{
				continue;
			}

			auto item = _stream_info_outputs.find(stream_track.first);
			if(item == _stream_info_outputs.end())
			{
				OV_ASSERT2(false);
				continue;
			}

			auto new_track = std::make_shared<MediaTrack>(*bypass_track.second);
			new_track->SetId(bypass_track.first);
			new_track->SetBypass(true);
			item->second->AddTrack(new_track);
			logti("stream_name(%s), track_id(%d)", stream_track.first.CStr(), bypass_track.first);
		}	
	}

	// Generate unused track list to delete
	for (auto context : _output_contexts)
	{
		bool found = false;
		for (auto stream_track : _stream_tracks)
		{
			auto it = find(stream_track.second.begin(), stream_track.second.end(), context.first);
			if (it != stream_track.second.end())
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			tracks.push_back(context.first);
		}
	}

	// Delete unused context
	for (auto track : tracks)
	{
		_output_contexts.erase(track);
	}

	///////////////////////////////////////////////////////
	// 트랜스코딩된 스트림을 생성함
	///////////////////////////////////////////////////////

	// Copy tracks (video & audio)
	for (auto &track : _stream_info_input->GetTracks())
	{
		// 기존 스트림의 미디어 트랙 정보
		auto &cur_track = track.second;
		CreateEncoders(cur_track);
	}

	CreateStreams();

	if(_encoders.empty())
	{
		return;
	}

	// _max_queue_size : 255
	_max_queue_size = (_encoders.size() > 0x0F) ? 0xFF : _encoders.size() * 256;

	logti("Transcoder Information / Encoders(%d) / Streams(%d)", _encoders.size(), _stream_tracks.size());

	// 패킷 저리 스레드 생성
	try
	{
		_kill_flag = false;

		_thread_decode = std::thread(&TranscodeStream::DecodeTask, this);
		_thread_filter = std::thread(&TranscodeStream::FilterTask, this);
		_thread_encode = std::thread(&TranscodeStream::EncodeTask, this);
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
	logtd("Destroyed Transcode Stream.  name(%s) id(%u)", _stream_info_input->GetName().CStr(), _stream_info_input->GetId());

	// 스레드가 종료가 안된경우를 확인해서 종료함
	if (_kill_flag != true)
	{
		Stop();
	}
}

void TranscodeStream::Stop()
{
	_kill_flag = true;

	logtd("wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag ? "true" : "false");

	_queue.abort();

	if (_thread_decode.joinable())
	{
		_thread_decode.join();
	}
	_queue_decoded.abort();

	if (_thread_filter.joinable())
	{
		_thread_filter.join();
	}
	_queue_filterd.abort();

	if (_thread_encode.joinable())
	{
		_thread_encode.join();
	}

	// Delete the streams here since if all streams are bypassed the TranscodeStream::DecodeTask(),
	// which previously deleted the streams won't be run
	DeleteStreams();
}

bool TranscodeStream::Push(std::unique_ptr<MediaPacket> packet)
{
	// logtd("Stage-1-1 : %f", (float)frame->GetPts());
	// 변경된 스트림을 큐에 넣음

	std::unique_ptr<MediaPacket> packet_for_encoding;
	bool no_encoders = _encoders.empty();
	bool pushed = false;
	auto it = _bypass_routes.find(packet->GetTrackId());
	if (it != _bypass_routes.end())
	{
		if (no_encoders == false)
		{
			packet_for_encoding = packet->ClonePacket();
		}
		packet->SetTrackId(it->second);
		SendFrame(std::move(packet), it->second);
		pushed = true;
	}
	else
	{
		packet_for_encoding = std::move(packet);
	}
	
	if(no_encoders)
	{
		return pushed;
	}

	auto input_tracks = _stream_info_input->GetTracks();
	auto input_track_iterator = input_tracks.find(packet_for_encoding->GetTrackId());
	if (input_track_iterator == input_tracks.end())
	{
		return false;
	}

	const auto media_type = packet_for_encoding->GetMediaType();
	const auto media_codec = input_track_iterator->second->GetCodecId();
	// Fragmentation headers need to be reset if any modifications to the bitstreams are performed
	if(media_type == common::MediaType::Video && media_codec == common::MediaCodecId::H264)
	{
		int64_t cts = 0;

		if(_bsfv.Convert(packet_for_encoding->GetData(), cts) == false)
		{
			return false;
		}

		// The timestamp used by RTMP is DTS
		// cts = pts - dts
		// pts = dts + cts
		// Convert timebase 1/1000 to 1/90000
		packet_for_encoding->SetPts(packet_for_encoding->GetDts() + (cts * 90));

		OV_ASSERT2(packet_for_encoding->GetPts() >= 0LL);

		packet_for_encoding->SetFragHeader(nullptr);
	}
	else if(media_type == common::MediaType::Video && media_codec == common::MediaCodecId::Vp8)
	{
		_bsf_vp8.convert_to(packet_for_encoding->GetData());

		packet_for_encoding->SetFragHeader(nullptr);
	}
	else if(media_type == common::MediaType::Audio && media_codec == common::MediaCodecId::Aac)
	{
		_bsfa.convert_to(packet_for_encoding->GetData());

		logtp("Enqueue for AAC\n%s", packet_for_encoding->GetData()->Dump(32).CStr());

		packet_for_encoding->SetFragHeader(nullptr);
	}
	else if(media_type == common::MediaType::Audio && media_codec == common::MediaCodecId::Opus)
	{
		// logtw("%s", buffer->GetData()->Dump(32).CStr());
		// _bsfa.convert_to(buffer.GetBuffer());
		logtp("Enqueue for OPUS\n%s", packet_for_encoding->GetData()->Dump(32).CStr());
	}
	else
	{
		OV_ASSERT2(false);
	}

	if(_queue.size() > _max_queue_size)
	{
		logti("Queue(stream) is full, please check your system: (queue: %zu > limit: %llu)", _queue.size(), _max_queue_size);
		return false;
	}

	_queue.push(std::move(packet_for_encoding));

	return true;
}

// std::unique_ptr<MediaFrame> TranscodeStream::Pop()
// {
// 	return _queue.pop_unique();
// }

void TranscodeStream::CreateDecoder(int32_t media_track_id, std::shared_ptr<TranscodeContext> input_context)
{
	// input_context must be decoding context
	OV_ASSERT2(input_context != nullptr);
	OV_ASSERT2(input_context->IsEncodingContext() == false);

	auto track = _stream_info_input->GetTrack(media_track_id);

	if (track == nullptr)
	{
		logte("media track allocation failed");

		return;
	}

	// create decoder for codec id
	auto decoder = std::move(TranscodeDecoder::CreateDecoder(track->GetCodecId(), input_context));
	if (decoder != nullptr)
	{
		_decoders[media_track_id] = std::move(decoder);
	}
}

void TranscodeStream::CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context)
{
	if (media_track == nullptr)
	{
		return;
	}

	// create encoder for codec id
	auto encoder = std::move(TranscodeEncoder::CreateEncoder(media_track->GetCodecId(), output_context));
	if (encoder != nullptr)
	{
		_encoders[media_track->GetId()] = std::move(encoder);
	}
}

void TranscodeStream::ChangeOutputFormat(MediaFrame *buffer)
{
	if (buffer == nullptr)
	{
		logte("Invalid media buffer");
		return;
	}

	int32_t track_id = buffer->GetTrackId();

	// 트랙 정보
	auto &track = _stream_info_input->GetTrack(track_id);

	if (track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", track_id);

		return;
	}

	CreateFilters(track, buffer);
}

TranscodeResult TranscodeStream::DecodePacket(int32_t track_id, std::unique_ptr<const MediaPacket> packet)
{
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

				if (_stats_decoded_frame_count % 300 == 0)
				{
					logtd("Decode stats: queue(%d), decoded_queue(%d), filtered_queue(%d)", _queue.size(), _queue_decoded.size(), _queue_filterd.size());
				}

				if (_queue_decoded.size() > _max_queue_size)
				{
					logti("Decoded frame queue is full, please check your system");
					return result;
				}

				_queue_decoded.push(std::move(decoded_frame));

				break;

			default:
				// An error occurred
				// There is no frame to process
				return result;
		}
	}
}

TranscodeResult TranscodeStream::FilterFrame(int32_t track_id, std::unique_ptr<MediaFrame> frame)
{
	auto filter_item = _filters.find(track_id);
	if (filter_item == _filters.end())
	{
		return TranscodeResult::NoData;
	}
	auto filter = filter_item->second.get();

	logtp("[#%d] Trying to apply a filter to the frame (PTS: %lld)", track_id, frame->GetPts());
	filter->SendBuffer(std::move(frame));

	while (true)
	{
		TranscodeResult result;
		auto filtered_frame = filter->RecvBuffer(&result);

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		switch (result)
		{
			case TranscodeResult::DataReady:
				filtered_frame->SetTrackId(track_id);

				logtp("[#%d] A frame is filtered (PTS: %lld)", track_id, filtered_frame->GetPts());

				if (_queue_filterd.size() > _max_queue_size)
				{
					_stats_queue_full_count++;

					if (_stats_queue_full_count % 256 == 0)
					{
						logti("Filtered frame queue is full, please decrease encoding options (resolution, bitrate, framerate)");
					}

					return result;
				}

				_queue_filterd.push(std::move(filtered_frame));

				break;

			default:
				return result;
		}
	}
}

TranscodeResult TranscodeStream::EncodeFrame(int32_t track_id, std::unique_ptr<const MediaFrame> frame)
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
void TranscodeStream::DecodeTask()
{
	logtd("Started transcode stream decode thread");

	while (!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto packet = _queue.pop_unique();
		if (packet == nullptr)
		{
			// logtw("invliad media buffer");
			continue;
		}

		// 패킷의 트랙 아이디를 조회
		int32_t track_id = packet->GetTrackId();

		DecodePacket(track_id, std::move(packet));
	}

	logtd("Terminated transcode stream decode thread");
}

void TranscodeStream::FilterTask()
{
	logtd("Transcode filter thread is started");

	while (!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto frame = _queue_decoded.pop_unique();
		if (frame == nullptr)
		{
			// logtw("invliad media buffer");
			continue;
		}

		DoFilters(std::move(frame));
	}

	logtd("Transcode filter thread is terminated");
}

void TranscodeStream::EncodeTask()
{
	logtd("Started transcode stream encode thread");

	while (!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto frame = _queue_filterd.pop_unique();
		if (frame == nullptr)
		{
			// logtw("invliad media buffer");
			continue;
		}

		// 패킷의 트랙 아이디를 조회
		int32_t track_id = frame->GetTrackId();

		// logtd("Stage-1-2 : %f", (float)frame->GetPts());
		EncodeFrame(track_id, std::move(frame));
	}

	logtd("Terminated transcode stream encode thread");
}

bool TranscodeStream::AddStreamInfoOutput(ov::String stream_name)
{
	auto stream_list = _stream_list.find(_application_info->GetId());
	if (stream_list == _stream_list.end())
	{
		// This code cannot happen.
		return false;
	}

	auto stream = stream_list->second.find(stream_name);
	if (stream != stream_list->second.end())
	{
		logtw("Output stream with the same name (%s) already exists", stream_name.CStr());
		return false;
	}
	stream_list->second.insert(stream_name);

	auto stream_info_output = std::make_shared<StreamInfo>();
	stream_info_output->SetName(stream_name);

	_stream_info_outputs.insert(
		std::make_pair(stream_name, stream_info_output));
	return true;
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
	_stream_list[_application_info->GetId()].clear();
}

void TranscodeStream::SendFrame(std::unique_ptr<MediaPacket> packet)
{
	uint8_t track_id = static_cast<uint8_t>(packet->GetTrackId());

	SendFrame(std::move(packet), track_id);
}

void TranscodeStream::SendFrame(std::unique_ptr<MediaPacket> packet, uint8_t track_id)
{
	for(auto &iter : _stream_info_outputs)
	{
		auto stream_track = _stream_tracks.find(iter.first);
		if (stream_track == _stream_tracks.end())
		{
			continue;
		}

		auto it = find(stream_track->second.begin(), stream_track->second.end(), track_id);
		if (it == stream_track->second.end())
		{
			continue;
		}

		auto clone_packet = packet->ClonePacket();
		_parent->SendFrame(iter.second, std::move(clone_packet));
	}
}

void TranscodeStream::CreateEncoders(std::shared_ptr<MediaTrack> media_track)
{
	for (auto &iter : _output_contexts)
	{
		if (media_track->GetMediaType() != iter.second->GetMediaType())
		{
			continue;
		}

		auto new_track = std::make_shared<MediaTrack>();
		new_track->SetId((uint32_t)iter.first);
		new_track->SetMediaType(media_track->GetMediaType());
		new_track->SetCodecId(iter.second->GetCodecId());
		new_track->SetTimeBase(iter.second->GetTimeBase().GetNum(), iter.second->GetTimeBase().GetDen());
		new_track->SetBitrate(iter.second->GetBitrate());

		if (media_track->GetMediaType() == common::MediaType::Video)
		{
			new_track->SetWidth(iter.second->GetVideoWidth());
			new_track->SetHeight(iter.second->GetVideoHeight());
			new_track->SetFrameRate(iter.second->GetFrameRate());
		}
		else if (media_track->GetMediaType() == common::MediaType::Audio)
		{
			new_track->SetSampleRate(iter.second->GetAudioSampleRate());
			new_track->GetSample().SetFormat(iter.second->GetAudioSample().GetFormat());
			new_track->GetChannel().SetLayout(iter.second->GetAudioChannel().GetLayout());
		}
		else
		{
			OV_ASSERT2(false);
			continue;
		}

		for (auto stream_track : _stream_tracks)
		{
			auto it = find(stream_track.second.begin(), stream_track.second.end(), iter.first);
			if (it == stream_track.second.end())
			{
				continue;
			}

			auto item = _stream_info_outputs.find(stream_track.first);
			if (item == _stream_info_outputs.end())
			{
				OV_ASSERT2(false);
				continue;
			}
			item->second->AddTrack(new_track);
			logti("stream_name(%s), track_id(%d)", stream_track.first.CStr(), iter.first);
		}

		CreateEncoder(new_track, iter.second);
	}
}

void TranscodeStream::CreateFilters(std::shared_ptr<MediaTrack> media_track, MediaFrame *buffer)
{
	for (auto &context_item : _output_contexts)
	{
		if (media_track->GetMediaType() != context_item.second->GetMediaType())
		{
			continue;
		}

		std::shared_ptr<TranscodeContext> input_context;

		if (media_track->GetMediaType() == common::MediaType::Video)
		{
			media_track->SetWidth(buffer->GetWidth());
			media_track->SetHeight(buffer->GetHeight());
			media_track->SetFormat(buffer->GetFormat());

			input_context = std::make_shared<TranscodeContext>(
				false,
				media_track->GetCodecId(),
				media_track->GetBitrate(),
				media_track->GetWidth(), media_track->GetHeight(),
				media_track->GetFrameRate());

			input_context->SetTimeBase(media_track->GetTimeBase());
		}
		else if (media_track->GetMediaType() == common::MediaType::Audio)
		{
			media_track->SetSampleRate(buffer->GetSampleRate());
			media_track->GetSample().SetFormat(buffer->GetFormat<common::AudioSample::Format>());
			media_track->GetChannel().SetLayout(buffer->GetChannelLayout());

			input_context = std::make_shared<TranscodeContext>(
				false,
				media_track->GetCodecId(),
				media_track->GetBitrate(),
				media_track->GetSampleRate());

			input_context->SetTimeBase(media_track->GetTimeBase());
			input_context->SetAudioSample(media_track->GetSample());
			input_context->GetAudioChannel().SetLayout(buffer->GetChannelLayout());
		}
		else
		{
			OV_ASSERT2(false);
			continue;
		}

		_filters[context_item.first] = std::make_unique<TranscodeFilter>(media_track, std::move(input_context), context_item.second);
	}
}

void TranscodeStream::DoFilters(std::unique_ptr<MediaFrame> frame)
{
	// 패킷의 트랙 아이디를 조회
	int32_t track_id = frame->GetTrackId();

	for (auto &iter : _output_contexts)
	{
		if (track_id != (int32_t)iter.second->GetMediaType())
		{
			continue;
		}

		auto frame_clone = frame->CloneFrame();
		if (frame_clone == nullptr)
		{
			logte("FilterTask -> Unknown Frame");
			continue;
		}

		FilterFrame(iter.first, std::move(frame_clone));
	}
}

uint8_t TranscodeStream::AddOutputContext(common::MediaType media_type, std::shared_ptr<TranscodeContext> output_context)
{	
	// output_context must be encoding context
	OV_ASSERT2(output_context != nullptr);
	OV_ASSERT2(output_context->IsEncodingContext());

	const auto index = GetTrackId(media_type);
	_output_contexts[index] = output_context;
	return index;
}

uint8_t TranscodeStream::GetTrackId(common::MediaType media_type)
{
	uint8_t last_index = 0;

	// 96-127 dynamic : RTP Payload Types for standard audio and video encodings
	if (media_type == common::MediaType::Video)
	{
		// 0x60 ~ 0x6F
		if (_last_track_video >= 0x70)
		{
			logte("The number of video encoders that can be supported is 16");
			return 0;
		}
		last_index = _last_track_video;
		++_last_track_video;
	}
	else if (media_type == common::MediaType::Audio)
	{
		// 0x70 ~ 0x7F
		if (_last_track_audio > 0x7F)
		{
			logte("The number of audio encoders that can be supported is 16");
			return 0;
		}
		last_index = _last_track_audio;
		++_last_track_audio;
	}
	return last_index;
}
