//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_aac.h"

#define OV_LOG_TAG "TranscodeCodec"

bool OvenCodecImplAvcodecEncAAC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	// find the MPEG-1 video decoder
	//AVCodec *codec = avcodec_find_encoder(GetCodecID());
	AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
	// ffmpeg 호환성 문제로 libfdk_aac 라이브러리 downgrade 필요
	// ffmpeg 빌드 옵션 : --enable-nonfree --enable-libfdk-aac --enable-encoder=libfdk_aac

	if(!codec)
	{
		logte("Codec not found");
		return false;
	}

	// create codec context
	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		logte("Could not allocate audio codec context");
		return false;
	}

	// put sample parameters
	_context->bit_rate = context->GetBitrate();

	// check that the encoder supports s16 pcm input
	// AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_NO
	_context->sample_fmt = AV_SAMPLE_FMT_S16;

	// select other audio parameters supported by the encoder
	// 지원 가능한 샘플 레이트 48000, 24000, 16000, 12000, 8000, 0
	_context->sample_rate = context->GetAudioSampleRate();
	_context->channel_layout = AV_CH_LAYOUT_STEREO;
	_context->channels = av_get_channel_layout_nb_channels(_context->channel_layout);

	_context->time_base = (AVRational){ 1, 1000 };

	_context->codec = codec;
	_context->codec_id = GetCodecID();

	// open codec
	if(avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAAC::RecvBuffer(TranscodeResult *result)
{
	// Check frame is availble
	int ret = avcodec_receive_packet(_context, _pkt);

	if(ret == AVERROR(EAGAIN))
	{
		// Packet is enqueued to encoder's buffer
	}
	else if(ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if(ret < 0)
	{
		logte("Error receiving a packet for encoding : %d", ret);

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		_coded_frame_count++;
		_coded_data_size += _pkt->size;

		auto packet_buffer = std::make_unique<MediaPacket>(common::MediaType::Audio, 1, _pkt->data, _pkt->size, _pkt->dts, MediaPacketFlag::Key);

		av_packet_unref(_pkt);

		*result = TranscodeResult::DataReady;
		return std::move(packet_buffer);


		//logtd("encoded audio packet %10.0f size=%10d flags=%5d", (float)_pkt->pts, _pkt->size, _pkt->flags);
		//*result = TranscodeResult::DataReady;
		//return nullptr;
	}

	// TEMP

	// TODO : 차후에 정식 구현 필요 - 버퍼처리 및 다양한 fmt 지원 필요
	// - Samle rate 변경으로 PCM 데이터 개수 정리 위해
	// - ex) 44100 -> 48000  변경 하면 nb_samples  1024 -> 1098
	// - 버퍼에 보관 이후 1024처리
	// - pts 조정 필요

	// Try encode
	while(_input_buffer.empty() == false)
	{
		// Dequeue the frame
		auto buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = buffer.get();

		_frame->nb_samples 		= _context->frame_size;
		_frame->format 			= _context->sample_fmt;
		_frame->channel_layout 	= _context->channel_layout;
		_frame->channels 		= _context->channels;
		_frame->sample_rate 	= _context->sample_rate;
		_frame->pts 			= frame->GetPts();

		if(av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));

		ret = avcodec_send_frame(_context, _frame);

		if(ret < 0)
		{
			logte("Error sending a frame for encoding: %d", ret);
		}

        	av_frame_unref(_frame);

	}

    	*result = TranscodeResult::NoData;
	return nullptr;
}
