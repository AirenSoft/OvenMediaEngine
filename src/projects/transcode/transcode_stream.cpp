//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <unistd.h>

#include "transcode_stream.h"
#include "transcode_application.h"
#include "utilities.h"

#include <config/config_manager.h>

#define OV_LOG_TAG "TranscodeStream"

common::MediaCodecId GetCodecId(ov::String name)
{
	name.MakeUpper();

	// Video codecs
	if(name == "H264")
	{
		return common::MediaCodecId::H264;
	}
	else if(name == "VP8")
	{
		return common::MediaCodecId::Vp8;
	}
	else if(name == "VP9")
	{
		return common::MediaCodecId::Vp9;
	}

	// Audio codecs
	if(name == "FLV")
	{
		return common::MediaCodecId::Flv;
	}
	else if(name == "AAC")
	{
		return common::MediaCodecId::Aac;
	}
	else if(name == "MP3")
	{
		return common::MediaCodecId::Mp3;
	}
	else if(name == "OPUS")
	{
		return common::MediaCodecId::Opus;
	}

	return common::MediaCodecId::None;
}

int GetBitrate(ov::String bitrate)
{
	bitrate.MakeUpper();

	int multiplier = 1;

	if(bitrate.HasSuffix("K"))
	{
		multiplier = 1024;
	}
	else if(bitrate.HasSuffix("M"))
	{
		multiplier = 1024 * 1024;
	}

	return static_cast<int>(ov::Converter::ToFloat(bitrate) * multiplier);
}

TranscodeStream::TranscodeStream(const info::Application &application_info, std::shared_ptr<StreamInfo> stream_info, TranscodeApplication *parent)
	: _application_info(application_info)
{
	logtd("Created Transcode stream. name(%s)", stream_info->GetName().CStr());

	// 통계 정보 초기화
	_stats_decoded_frame_count = 0;

	// maximum queue size
	_max_queue_size = 0;

	_stats_queue_full_count = 0;

	// 부모 클래스
	_parent = parent;

	// 입력 스트림 정보
	_stream_info_input = stream_info;

	// Prepare decoders
	for(auto &track : _stream_info_input->GetTracks())
	{
		CreateDecoder(track.second->GetId());
	}

    if (_decoders.empty()) {
        return;
    }

	// Generate track list by profile(=encode name)
	auto encodes = _application_info.GetEncodes();
	std::map <ov::String, std::vector <uint8_t >> profile_tracks;
	std::vector <uint8_t> tracks;

	for(const auto &encode : encodes)
	{
		if(!encode.IsActive())
		{
			continue;
		}

		auto video_profile = encode.GetVideoProfile();
		auto audio_profile = encode.GetAudioProfile();
		auto profile_name = encode.GetName();

		if((video_profile != nullptr) && (video_profile->IsActive()))
		{
			auto context = std::make_shared<TranscodeContext>(
				GetCodecId(video_profile->GetCodec()),
				GetBitrate(video_profile->GetBitrate()),
				video_profile->GetWidth(), video_profile->GetHeight(),
				video_profile->GetFramerate()
			);
			uint8_t track_id = AddContext(common::MediaType::Video, context);
			if(track_id)
			{
				tracks.push_back(track_id);
			}
		}

		if((audio_profile != nullptr) && (audio_profile->IsActive()))
		{
			auto context = std::make_shared<TranscodeContext>(
				GetCodecId(audio_profile->GetCodec()),
				GetBitrate(audio_profile->GetBitrate()),
				audio_profile->GetSamplerate()
			);
			uint8_t track_id = AddContext(common::MediaType::Audio, context);
			if(track_id)
			{
				tracks.push_back(track_id);
			}
		}

		if (!tracks.empty()) {
			profile_tracks[profile_name] = tracks;
			tracks.clear();
		}
	}

	// Generate track list by stream
	auto streams = _application_info.GetStreamList();

	for(const auto &stream : streams)
	{
		auto stream_name = stream.GetName();
		stream_name = stream_name.Replace("${OriginStreamName}", stream_info->GetName());

		if(!AddStreamInfoOutput(stream_name))
		{
			continue;
		}

		auto profiles = stream.GetProfiles();

		for(const auto &profile : profiles)
		{
			auto item = profile_tracks.find(profile.GetName());
			if(item == profile_tracks.end())
			{
				continue;
			}
			tracks.push_back(item->second[0]);  // Video
			tracks.push_back(item->second[1]);  // Audio
		}
		_stream_tracks[stream_name] = tracks;
		tracks.clear();
	}

	// Generate unused track list to delete
	for (auto context : _contexts)
	{
		bool found = false;
		for (auto stream_track : _stream_tracks)
		{
			auto it = find(stream_track.second.begin(), stream_track.second.end(), context.first);
			if(it != stream_track.second.end())
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			tracks.push_back(context.first);
		}
	}

	// Delete unused context
	for (auto track : tracks)
	{
		_contexts.erase(track);
	}

	///////////////////////////////////////////////////////
	// 트랜스코딩된 스트림을 생성함
	///////////////////////////////////////////////////////

	// Copy tracks (video & audio)
	for(auto &track : _stream_info_input->GetTracks())
	{
		// 기존 스트림의 미디어 트랙 정보
		auto &cur_track = track.second;
		CreateEncoders(cur_track);
	}

	if (_encoders.empty()) {
		return;
	}

	// _max_queue_size : 255
	_max_queue_size = (_encoders.size() > 0x0F) ? 0xFF : _encoders.size() * 16;

	logti("Transcoder Information / Encoders(%d) / Streams(%d)", _encoders.size(), _stream_tracks.size());

	// 패킷 저리 스레드 생성
	try
	{
		_kill_flag = false;

		_thread_decode = std::thread(&TranscodeStream::DecodeTask, this);
		_thread_filter = std::thread(&TranscodeStream::FilterTask, this);
		_thread_encode = std::thread(&TranscodeStream::EncodeTask, this);
	}
	catch(const std::system_error &e)
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
	if(_kill_flag != true)
	{
		Stop();
	}
}

void TranscodeStream::Stop()
{
	_kill_flag = true;

	logtd("wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag ? "true" : "false");

	_queue.abort();

	if (_thread_decode.joinable()) {
        _thread_decode.join();
    }
	_queue_decoded.abort();

	if (_thread_filter.joinable()) {
        _thread_filter.join();
    }
	_queue_filterd.abort();

	if (_thread_encode.joinable()) {
        _thread_encode.join();
    }
}

bool TranscodeStream::Push(std::unique_ptr<MediaPacket> packet)
{
	// logtd("Stage-1-1 : %f", (float)frame->GetPts());
	// 변경된 스트림을 큐에 넣음

	if (_encoders.empty()) {
        return false;
	}

	if(_queue.size() > _max_queue_size)
	{
		logti("Queue(stream) is full, please check your system");
		return false;
	}

	_queue.push(std::move(packet));

	return true;
}

// std::unique_ptr<MediaFrame> TranscodeStream::Pop()
// {
// 	return _queue.pop_unique();
// }

void TranscodeStream::CreateDecoder(int32_t media_track_id)
{
	auto track = _stream_info_input->GetTrack(media_track_id);

	if(track == nullptr)
	{
		logte("media track allocation failed");

		return;
	}

	// create decoder for codec id
	auto decoder = std::move(TranscodeDecoder::CreateDecoder(track->GetCodecId()));
	if (decoder != nullptr)
	{
        _decoders[media_track_id] = std::move(decoder);
	}
}

void TranscodeStream::CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> transcode_context)
{
	if(media_track == nullptr)
	{
		return;
	}

	// create encoder for codec id
	auto encoder = std::move(TranscodeEncoder::CreateEncoder(media_track->GetCodecId(), transcode_context));
	if (encoder != nullptr)
	{
        _encoders[media_track->GetId()] = std::move(encoder);
    }
}

void TranscodeStream::ChangeOutputFormat(MediaFrame *buffer)
{
	if(buffer == nullptr)
	{
		logte("invalid media buffer");
		return;
	}

	int32_t track_id = buffer->GetTrackId();

	// 트랙 정보
	auto &track = _stream_info_input->GetTrack(track_id);
	if(track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", track_id);

		return;
	}

	CreateFilters(track, buffer);
}

TranscodeResult TranscodeStream::do_decode(int32_t track_id, std::unique_ptr<const MediaPacket> packet)
{
	////////////////////////////////////////////////////////
	// 1) 디코더에 전달함
	////////////////////////////////////////////////////////
	auto decoder_item = _decoders.find(track_id);

	if(decoder_item == _decoders.end())
	{
		return TranscodeResult::NoData;
	}

	TranscodeDecoder *decoder = decoder_item->second.get();
	decoder->SendBuffer(std::move(packet));

	while(true)
	{
		TranscodeResult result;
		auto ret_frame = decoder->RecvBuffer(&result);

		switch(result)
		{
			case TranscodeResult::FormatChanged:
				// output format change 이벤트가 발생하면...
				// filter 및 인코더를 여기서 다시 초기화 해줘야함.
				// 디코더에 의해서 포맷 정보가 새롭게 알게되거나, 변경됨을 나타내를 반환값

				// logte("changed output format");
				// 필터 컨테스트 생성
				// 인코더 커테스트 생성
				ret_frame->SetTrackId(track_id);

				ChangeOutputFormat(ret_frame.get());
				break;

			case TranscodeResult::DataReady:
				// 디코딩이 성공하면,
				ret_frame->SetTrackId(track_id);

				// 필터 단계로 전달
				// do_filter(track_id, std::move(ret_frame));

				_stats_decoded_frame_count++;

				if(_stats_decoded_frame_count % 300 == 0)
				{
					logtd("stats. rq(%d), dq(%d), fq(%d)", _queue.size(), _queue_decoded.size(), _queue_filterd.size());
				}

				if(_queue_decoded.size() > _max_queue_size)
				{
					logti("Queue(decoded) is full, please check your system");
					return result;
				}

				_queue_decoded.push(std::move(ret_frame));

				break;

			default:
				// 에러, 또는 디코딩된 패킷이 없다면 종료
				return result;
		}
	}
}

TranscodeResult TranscodeStream::do_filter(int32_t track_id, std::unique_ptr<MediaFrame> frame)
{
	////////////////////////////////////////////////////////
	// 1) 디코더에 전달함
	////////////////////////////////////////////////////////
	auto filter = _filters.find(track_id);

	if(filter == _filters.end())
	{
		return TranscodeResult::NoData;
	}

	logtp("SendBuffer to do_filter()\n%s", ov::Dump(frame->GetBuffer(0), frame->GetBufferSize(0), 32).CStr());

	filter->second->SendBuffer(std::move(frame));

	while(true)
	{
		TranscodeResult result;
		auto ret_frame = filter->second->RecvBuffer(&result);

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		switch(result)
		{
			case TranscodeResult::DataReady:
				ret_frame->SetTrackId(track_id);

				logtp("Received from filter:\n%s", ov::Dump(ret_frame->GetBuffer(0), ret_frame->GetBufferSize(0), 32).CStr());

				if (_queue_filterd.size() > _max_queue_size)
				{
					if(++_stats_queue_full_count % 256 == UINT8_MAX)
					{
						logti("Queue(filtered) is full, please decrease encoding options(resolution,bitrate,framerate)");
					}
					return result;
				}
				_queue_filterd.push(std::move(ret_frame));
				break;

			default:
				return result;
		}
	}
}

TranscodeResult TranscodeStream::do_encode(int32_t track_id, std::unique_ptr<const MediaFrame> frame)
{
	if(_encoders.find(track_id) == _encoders.end())
	{
		return TranscodeResult::NoData;
	}

	////////////////////////////////////////////////////////
	// 2) 인코더에 전달
	////////////////////////////////////////////////////////

	auto &encoder = _encoders[track_id];
	encoder->SendBuffer(std::move(frame));

	while(true)
	{
		TranscodeResult result;
		auto ret_packet = encoder->RecvBuffer(&result);

		if(static_cast<int>(result) < 0)
		{
			return result;
		}

		if(result == TranscodeResult::DataReady)
		{
			ret_packet->SetTrackId(track_id);

			// logtd("encoded packet. track_id(%d), pts(%.0f)", ret_packet->GetTrackId(), (float)ret_packet->GetPts());

			////////////////////////////////////////////////////////
			// 3) 미디어 라우더에 전달
			////////////////////////////////////////////////////////
			SendFrame(std::move(ret_packet));
		}
	}
}

// 디코딩 & 인코딩 스레드
void TranscodeStream::DecodeTask()
{
	CreateStreams();

	logtd("Started transcode stream decode thread");

	while(!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto packet = _queue.pop_unique();
		if(packet == nullptr)
		{
			// logtw("invliad media buffer");
			continue;
		}

		// 패킷의 트랙 아이디를 조회
		int32_t track_id = packet->GetTrackId();

		do_decode(track_id, std::move(packet));
	}

	// 스트림 삭제 전송
	DeleteStreams();

	logtd("Terminated transcode stream decode thread");
}

void TranscodeStream::FilterTask()
{
	logtd("Transcode filter thread is started");

	while(!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto frame = _queue_decoded.pop_unique();
		if(frame == nullptr)
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

	while(!_kill_flag)
	{
		// 큐에 있는 인코딩된 패킷을 읽어옴
		auto frame = _queue_filterd.pop_unique();
		if(frame == nullptr)
		{
			// logtw("invliad media buffer");
			continue;
		}

		// 패킷의 트랙 아이디를 조회
		int32_t track_id = frame->GetTrackId();

		// logtd("Stage-1-2 : %f", (float)frame->GetPts());
		do_encode(track_id, std::move(frame));
	}

	logtd("Terminated transcode stream encode thread");
}

bool TranscodeStream::AddStreamInfoOutput(ov::String stream_name)
{
	auto stream_info_output = std::make_shared<StreamInfo>();
	stream_info_output->SetName(stream_name);

	if(!_stream_info_outputs.insert(
			std::make_pair(stream_name, stream_info_output)).second)
	{
		logtw("The stream [%s] already exists", stream_name.CStr());
		return false;
	}
	return true;
}

void TranscodeStream::CreateStreams()
{
	for(auto &iter : _stream_info_outputs)
	{
		_parent->CreateStream(iter.second);
	}
}

void TranscodeStream::DeleteStreams()
{
	for(auto &iter : _stream_info_outputs)
	{
		_parent->DeleteStream(iter.second);
	}
}

void TranscodeStream::SendFrame(std::unique_ptr<MediaPacket> packet)
{
	uint8_t track_id = static_cast<uint8_t>(packet->GetTrackId());

	for(auto &iter : _stream_info_outputs)
	{
		auto stream_track = _stream_tracks.find(iter.first);
		if(stream_track == _stream_tracks.end())
		{
			continue;
		}

		auto it = find(stream_track->second.begin(), stream_track->second.end(), track_id);
		if(it == stream_track->second.end())
		{
			continue;
		}

		auto clone_packet = packet->ClonePacket();
		_parent->SendFrame(iter.second, std::move(clone_packet));
	}
}

void TranscodeStream::CreateEncoders(std::shared_ptr<MediaTrack> media_track)
{
	for(auto &iter : _contexts)
	{
		if(media_track->GetMediaType() != iter.second->GetMediaType())
		{
			continue;
		}

		auto new_track = std::make_shared<MediaTrack>();
		new_track->SetId((uint32_t)iter.first);
		new_track->SetMediaType(media_track->GetMediaType());
		new_track->SetCodecId(iter.second->GetCodecId());
		new_track->SetTimeBase(iter.second->GetTimeBase().GetNum(), iter.second->GetTimeBase().GetDen());
        new_track->SetBitrate(iter.second->GetBitrate());

		if(media_track->GetMediaType() == common::MediaType::Video)
		{
			new_track->SetWidth(iter.second->GetVideoWidth());
			new_track->SetHeight(iter.second->GetVideoHeight());
			new_track->SetFrameRate(iter.second->GetFrameRate());

		}
		else if(media_track->GetMediaType() == common::MediaType::Audio)
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
			item->second->AddTrack(new_track);
			logti("stream_name(%s), track_id(%d)", stream_track.first.CStr(), iter.first);
		}

		// av_log_set_level(AV_LOG_DEBUG);
		CreateEncoder(new_track, iter.second);
	}
}

void TranscodeStream::CreateFilters(std::shared_ptr<MediaTrack> media_track, MediaFrame *buffer)
{
	for(auto &iter : _contexts)
	{
		if(media_track->GetMediaType() != iter.second->GetMediaType())
		{
			continue;
		}

		if(media_track->GetMediaType() == common::MediaType::Video)
		{
			media_track->SetWidth(buffer->GetWidth());
			media_track->SetHeight(buffer->GetHeight());
		}
		else if(media_track->GetMediaType() == common::MediaType::Audio)
		{
			media_track->SetSampleRate(buffer->GetSampleRate());
			media_track->GetSample().SetFormat(buffer->GetFormat<common::AudioSample::Format>());
			media_track->GetChannel().SetLayout(buffer->GetChannelLayout());
		}
		else
		{
			OV_ASSERT2(false);
			continue;
		}
		_filters[iter.first] = std::make_unique<TranscodeFilter>(media_track, iter.second);
	}
}

void TranscodeStream::DoFilters(std::unique_ptr<MediaFrame> frame)
{
	// 패킷의 트랙 아이디를 조회
	int32_t track_id = frame->GetTrackId();

	for(auto &iter: _contexts)
	{
		if(track_id != (int32_t)iter.second->GetMediaType())
		{
			continue;
		}

		auto frame_clone = frame->CloneFrame();
		if(frame_clone == nullptr)
		{
			logte("FilterTask -> Unknown Frame");
			continue;
		}
		do_filter(iter.first, std::move(frame_clone));
	}
}

uint8_t TranscodeStream::AddContext(common::MediaType media_type, std::shared_ptr<TranscodeContext> context)
{
	uint8_t last_index = 0;
	// 96-127 dynamic : RTP Payload Types for standard audio and video encodings
	if (media_type == common::MediaType::Video)
	{
		// 0x60 ~ 0x6F
		if(_last_track_video >= 0x70)
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
		if(_last_track_audio > 0x7F)
		{
			logte("The number of audio encoders that can be supported is 16");
			return 0;
		}
		last_index = _last_track_audio;
		++_last_track_audio;
	}
	_contexts[last_index] = context;

	return last_index;
}