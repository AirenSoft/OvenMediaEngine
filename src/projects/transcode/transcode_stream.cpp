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

#define OV_LOG_TAG "TranscodeStream"


TranscodeStream::TranscodeStream(std::shared_ptr<StreamInfo> stream_info, TranscodeApplication* parent) 
{
	logtd("Created Transcode stream. name(%s)", stream_info->GetName().CStr());

	// 통계 정보 초기화
	_stats_decoded_frame_count = 0;

	// 부모 클래스
	_parent = parent;

	// 입력 스트림 정보	
	_stream_info_input = stream_info;



	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 디코더 모듈을 활성화함
	///////////////////////////////////////////////////////////////////////////////////////////////////
	
	for(uint32_t track_index = 0 ; track_index < _stream_info_input->GetTrackCount() ; track_index ++)
	{
		auto track = _stream_info_input->GetTrackAt(track_index);
		if(track == nullptr)
		{
			logte("Cannot find track of stream.");
			continue;
		}

		CreateDecoder(track->GetId());
	}


	///////////////////////////////////////////////////////
	// 트랜스코딩을 설정으로부터 읽어온다
	///////////////////////////////////////////////////////
	_transcode_context = std::make_shared<TranscodeContext>();

	_transcode_context->SetVideoCodecId(MediaCodecId::CODEC_ID_VP8);
	
	_transcode_context->SetVideoBitrate(5000000);
	
	_transcode_context->SetVideoWidth(1920);
	
	_transcode_context->SetVideoHeight(1080);
	
	_transcode_context->SetFrameRate(30.00f);
	
	_transcode_context->SetGOP(30);

	_transcode_context->SetVideoTimeBase(1, 1000000);

	_transcode_context->SetAudioCodecId(MediaCodecId::CODEC_ID_OPUS);
	
	_transcode_context->SetAudioBitrate(64000);
	
	_transcode_context->SetAudioSampleRate(48000);
	
	_transcode_context->_audio_channel.SetLayout(AudioChannel::Layout::AUDIO_LAYOUT_STEREO); // STEREO

    _transcode_context->SetAudioSampleForamt(AudioSample::Format::SAMPLE_FMT_FLTP);

	_transcode_context->SetAudioTimeBase(1, 1000000);
    

	///////////////////////////////////////////////////////
	// 트랜스코딩된 스트림을 생성함
	///////////////////////////////////////////////////////
    // TODO: 트랜스 코딩 프로팡리이 여러개가 되는 경우 어떻게 처리할 것인가? TranscodeSteam 모듈에서 Output Stream Info를 여러개 생성해서 
    // 처리할지 구조적으로 검토를 해봐야함.
	_stream_info_output = std::make_shared<StreamInfo>();

	// 스트림명 설정
	_stream_info_output->SetName(ov::String::FormatString("%s_o", stream_info->GetName().CStr()));


	// 트랙 복사
	for(uint32_t track_index = 0 ; track_index < _stream_info_input->GetTrackCount() ; track_index ++)
	{
		// 기존 스트림의 미디어 트랙 정보
		auto cur_track = _stream_info_input->GetTrackAt(track_index);

		// 새로운 스트림의 트랙 정보
		auto new_track = std::make_shared<MediaTrack>();
	
		new_track->SetId(cur_track->GetId());
		
		new_track->SetMediaType(cur_track->GetMediaType());
		

		if(cur_track->GetMediaType() == MediaType::MEDIA_TYPE_VIDEO)
		{
			new_track->SetCodecId(_transcode_context->GetVideoCodecId());
		
			new_track->SetWidth(_transcode_context->GetVideoWidth());
		
			new_track->SetHeight(_transcode_context->GetVideoHeight());
		
			new_track->SetFrameRate(_transcode_context->GetFrameRate());
		
			new_track->SetTimeBase(_transcode_context->GetVideoTimeBase().GetNum(), _transcode_context->GetVideoTimeBase().GetDen());
		}
		else if(cur_track->GetMediaType() == MediaType::MEDIA_TYPE_AUDIO)
		{
			new_track->SetCodecId(_transcode_context->GetAudioCodecId());
		
			new_track->SetSampleRate(_transcode_context->GetAudioSampleRate());
		
			new_track->GetSample().SetFormat(_transcode_context->_audio_sample.GetFormat());
		
			new_track->GetChannel().SetLayout(_transcode_context->_audio_channel.GetLayout());

			new_track->SetTimeBase(_transcode_context->GetAudioTimeBase().GetNum(), _transcode_context->GetAudioTimeBase().GetDen());
		}
		else
		{
			continue;
		}

		_stream_info_output->AddTrack(new_track);

		// av_log_set_level(AV_LOG_DEBUG);
		CreateEncoder(new_track, _transcode_context);
	}


	// 패킷 저리 스레드 생성
	try 
	{
        _kill_flag = false;

		_thread_decode = std::thread(&TranscodeStream::DecodeTask, this);
		_thread_filter = std::thread(&TranscodeStream::FilterTask, this);
		_thread_encode = std::thread(&TranscodeStream::EncodeTask, this);
    } 
    catch (const std::system_error& e) 
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
	
	logtd("wait for terminated trancode stream thread. kill_flag(%s)", _kill_flag?"true":"false");
	_queue.abort();
	_thread_decode.join();	
	
	_queue_decoded.abort();

	_thread_filter.join();

	_queue_filterd.abort();
	
	_thread_encode.join();
}


std::shared_ptr<StreamInfo> TranscodeStream::GetStreamInfo()
{
	return _stream_info_input;
}


bool TranscodeStream::Push(std::unique_ptr<MediaBuffer> frame)
{
	// logtd("Stage-1-1 : %f", (float)frame->GetPts());
	// 변경된 스트림을 큐에 넣음
	_queue.push(std::move(frame));

	return true;
}

// std::unique_ptr<MediaBuffer> TranscodeStream::Pop()
// {
// 	return _queue.pop_unique();
// }

uint32_t TranscodeStream::GetBufferCount()
{
	return _queue.size();	
}

// 디코더를 생성함
void TranscodeStream::CreateDecoder(int32_t media_track_id)
{
	auto track = _stream_info_input->GetTrack(media_track_id);
	if(track == nullptr)
	{
		logte("media track allocation failed");

		return;
	}

	// 잊풋 트랙 아이디
	_decoders[media_track_id] = std::make_unique<TranscodeCodec>((MediaCodecId)track->GetCodecId(), false);
}

void TranscodeStream::CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> transcode_context)
{
	if(media_track == nullptr)
	{
		return;
	}

	// 인코더 생성. 키는 원본 스트림정보의 트랙 아이디
	_encoders[media_track->GetId()] = std::make_unique<TranscodeCodec>((MediaCodecId)media_track->GetCodecId(), true, transcode_context);
}

void TranscodeStream::ChangeOutputFormat(MediaBuffer* buffer)
{
	if(buffer == nullptr)
	{
		logte("invalid media buffer");
		return;
	}

	int32_t track_id = buffer->GetTrackId();

	// 트랙 정보
	auto track = _stream_info_input->GetTrackAt(track_id);
	if(track == nullptr)
	{
		logte("cannot find output media track. track_id(%d)", track_id);

		return;
	}
	
	if(track->GetMediaType() == MediaType::MEDIA_TYPE_VIDEO)
	{
		logtd("parsed form media buffer. width:%d, height:%d, format:%d", buffer->_width, buffer->_height, buffer->_format);

		track->SetWidth(buffer->_width);
		track->SetHeight(buffer->_height);
		track->GetTimeBase().Set(1, 1000);

		_filters[track->GetId()]  = std::make_unique<TranscodeFilter>(TranscodeFilter::FilterType::FILTER_TYPE_VIDEO_RESCALER, track, _transcode_context);
	}
	else if(track->GetMediaType() == MediaType::MEDIA_TYPE_AUDIO)
	{
		logtd("parsed form media buffer. format(%d), bytes_per_sample(%d), nb_samples(%d), channels(%d), channel_layout(%d), sample_rate(%d)"
			,buffer->_format
			,buffer->_bytes_per_sample
			,buffer->_nb_samples
			,buffer->_channels
			,buffer->_channel_layout
			,buffer->_sample_rate
			);
		
		track->SetSampleRate(buffer->_sample_rate);
		track->GetSample().SetFormat((AudioSample::Format)buffer->_format);
		track->GetChannel().SetLayout((AudioChannel::Layout)buffer->_channel_layout);
		track->GetTimeBase().Set(1, 1000);

		_filters[track->GetId()] = std::make_unique<TranscodeFilter>(TranscodeFilter::FilterType::FILTER_TYPE_AUDIO_RESAMPLER, track, _transcode_context);
	}
}


int32_t TranscodeStream::do_decode(int32_t track_id, std::unique_ptr<MediaBuffer> frame)
{
	////////////////////////////////////////////////////////
	// 1) 디코더에 전달함
	////////////////////////////////////////////////////////
	if(_decoders.find(track_id) == _decoders.end())
	{
		return -1;
	}

	_decoders[track_id]->SendBuffer(std::move(frame));

	while(true) {
		auto pair_object = _decoders[track_id]->RecvBuffer();
		int32_t ret_code = pair_object.first;
		auto ret_frame = std::move(pair_object.second);

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		if(ret_code < 0) 
		{
			return ret_code;
		}

		// output format change 이벤트가 발생하면... 
		// filter 및 인코더를 여기서 다시 초기화 해줘야함.
		// 디코더에 의해서 포맷 정보가 새롭게 알게되거나, 변경됨을 나타내를 반환값
		if(ret_code == 1)
		{
			// logte("changed output format");
			// 필터 컨테스트 생성
			// 인코더 커테스트 생성
			ret_frame->SetTrackId(track_id);

			ChangeOutputFormat(ret_frame.get());
		}
		// 디코딩이 성공하면,
		else if(ret_code == 0)
		{
			ret_frame->SetTrackId(track_id);

			// 필터 단계로 전달
			// do_filter(track_id, std::move(ret_frame));

			_stats_decoded_frame_count++;

			if(_stats_decoded_frame_count % 300 == 0)
				logtd("stats. rq(%d), dq(%d), fq(%d)", _queue.size(), _queue_decoded.size(), _queue_filterd.size());
			// logtd("Stage-1-2 : %f", (float)frame->GetPts());

			_queue_decoded.push(std::move(ret_frame));
		}
	}

	// 여기로 들어올 확율은 없음. 0.000001%
	return 0;
}

int32_t TranscodeStream::do_filter(int32_t track_id, std::unique_ptr<MediaBuffer> frame)
{
	////////////////////////////////////////////////////////
	// 1) 디코더에 전달함
	////////////////////////////////////////////////////////
	if(_filters.find(track_id) == _filters.end())
	{
		return -1;
	}

	_filters[track_id]->SendBuffer(std::move(frame));

	while(true) {
		auto pair_object = _filters[track_id]->RecvBuffer();
		int32_t ret_code = pair_object.first;
		auto ret_frame = std::move(pair_object.second);

		// 에러, 또는 디코딩된 패킷이 없다면 종료
		if(ret_code < 0) 
		{
			return ret_code;
		}

		// 틴터에 성공하면
		if(ret_code == 0)
		{
			ret_frame->SetTrackId(track_id);
			
			// logtd("filtered frame. track_id(%d), pts(%.0f)", ret_frame->GetTrackId(), (float)ret_frame->GetPts());

			_queue_filterd.push(std::move(ret_frame));
			// do_encode(track_id, std::move(ret_frame));
		}
	}

	return 0;
}

int32_t TranscodeStream::do_encode(int32_t track_id, std::unique_ptr<MediaBuffer> frame)
{
	if(_encoders.find(track_id) == _encoders.end())
	{
		return -1;
	}

	////////////////////////////////////////////////////////
	// 2) 인코더에 전달
	////////////////////////////////////////////////////////
	_encoders[track_id]->SendBuffer(std::move(frame));

	while(true) 
	{
		auto pair_encoded_object = _encoders[track_id]->RecvBuffer();

		int32_t ret_code = pair_encoded_object.first;

		if(ret_code < 0) 
		{
			return ret_code;
		}

		if(ret_code == 0)
		{
			auto ret_packet = std::move(pair_encoded_object.second);

			ret_packet->SetTrackId(track_id);

			// logtd("encoded packet. track_id(%d), pts(%.0f)", ret_packet->GetTrackId(), (float)ret_packet->GetPts());

			////////////////////////////////////////////////////////
			// 3) 미디어 라우더에 전달
			////////////////////////////////////////////////////////
			_parent->SendFrame(_stream_info_output, std::move(ret_packet));
		}
	}

	return 0;
}

// 디코딩 & 인코딩 스레드
void TranscodeStream::DecodeTask()
{
	// 스트림 생성 전송
	_parent->CreateStream(_stream_info_output);

	logtd("Started transcode stream decode thread");

    while(!_kill_flag) 
    {
    	// 큐에 있는 인코딩된 패킷을 읽어옴
    	auto frame = _queue.pop_unique();
    	if(frame == nullptr)
    	{
    		// logtw("invliad media buffer");
    		continue;
    	}

    	// 패킷의 트랙 아이디를 조회
		int32_t track_id = frame->GetTrackId();

		do_decode(track_id, std::move(frame));
    }

	// 스트림 삭제 전송
    _parent->DeleteStream(_stream_info_output);

	logtd("Terminated transcode stream decode thread");
}

void TranscodeStream::FilterTask()
{
	logtd("Started transcode stream  thread");

 	while(!_kill_flag) 
    {
    	// 큐에 있는 인코딩된 패킷을 읽어옴
    	auto frame = _queue_decoded.pop_unique();
    	if(frame == nullptr)
    	{
    		// logtw("invliad media buffer");
    		continue;
    	}

    	// 패킷의 트랙 아이디를 조회
		int32_t track_id = frame->GetTrackId();

		// logtd("Stage-1-2 : %f", (float)frame->GetPts());
		do_filter(track_id, std::move(frame));
    }

	logtd("Terminated transcode stream filter thread");
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
