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

bool OvenCodecImplAvcodecEncAAC::Configure(std::shared_ptr<TranscodeContext> output_context)
{
	if (TranscodeEncoder::Configure(output_context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();
	AVCodec *codec = ::avcodec_find_encoder(codec_id);
	// AVCodec *codec = ::avcodec_find_encoder_by_name("libfdk_aac");
	// ffmpeg 호환성 문제로 libfdk_aac 라이브러리 downgrade 필요
	// ffmpeg 빌드 옵션 : --enable-nonfree --enable-libfdk-aac --enable-encoder=libfdk_aac

	if (codec == nullptr)
	{
		logte("Codec not found: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// create codec context
	_context = ::avcodec_alloc_context3(codec);

	if (_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	_context->bit_rate = output_context->GetBitrate();
	_context->sample_fmt = AV_SAMPLE_FMT_S16;

	// Supported sample rate: 48000, 24000, 16000, 12000, 8000, 0
	_context->sample_rate = output_context->GetAudioSampleRate();
	_context->channel_layout = static_cast<uint64_t>(output_context->GetAudioChannel().GetLayout());
	_context->channels = output_context->GetAudioChannel().GetCounts();
	// _context->time_base = TimebaseToAVRational(output_context->GetTimeBase());
	_context->codec = codec;
	_context->codec_id = codec_id;

	// open codec
	if (::avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	return true;
}

std::shared_ptr<MediaPacket> OvenCodecImplAvcodecEncAAC::RecvBuffer(TranscodeResult *result)
{
	// Check frame is availble
	int ret = ::avcodec_receive_packet(_context, _packet);

	if (ret == AVERROR(EAGAIN))
	{
		// Wait for more packet
	}
	else if (ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if (ret < 0)
	{
		logte("Error receiving a packet for encoding : %d", ret);

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		if (_packet->pts >= 0LL)
		{
			// Packet is ready
			auto packet_buffer = std::make_shared<MediaPacket>(common::MediaType::Audio, 1, _packet->data, _packet->size, _packet->pts / 1000, _packet->dts / 1000, _packet->duration, MediaPacketFlag::Key);

			// logte("ENCODED:: %lld, %lld", packet_buffer->GetPts(), _packet->pts);
			::av_packet_unref(_packet);


			*result = TranscodeResult::DataReady;
			return std::move(packet_buffer);
		}
	}

	while (_input_buffer.empty() == false)
	{
		// Dequeue the frame
		auto buffer = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		const MediaFrame *frame = buffer.get();

		// logte("DECODE:: %lld %lld", frame->GetPts(), frame->GetPts() * 1000);

		_frame->format = _context->sample_fmt;
		_frame->nb_samples = _context->frame_size;
		_frame->pts = frame->GetPts() * 1000;
		_frame->pkt_duration = frame->GetDuration();

		_frame->channel_layout = _context->channel_layout;
		_frame->channels = _context->channels;
		_frame->sample_rate = _context->sample_rate;

		if (::av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));

		ret = ::avcodec_send_frame(_context, _frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding: %d", ret);
		}

		::av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
