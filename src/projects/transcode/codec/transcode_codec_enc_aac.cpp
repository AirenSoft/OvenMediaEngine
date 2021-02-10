//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_aac.h"

#include "../transcode_private.h"

OvenCodecImplAvcodecEncAAC::~OvenCodecImplAvcodecEncAAC()
{
	Stop();
}

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

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&OvenCodecImplAvcodecEncAAC::ThreadEncode, this);
		pthread_setname_np(_thread_work.native_handle(), "EncAAC");
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}

	return true;
}

void OvenCodecImplAvcodecEncAAC::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();
	_output_buffer.Stop();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("AAC encoder thread has ended.");
	}
}

void OvenCodecImplAvcodecEncAAC::ThreadEncode()
{
	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
			continue;

		auto buffer = std::move(obj.value());

		///////////////////////////////////////////////////
		// Request frame encoding to codec
		///////////////////////////////////////////////////

		const MediaFrame *frame = buffer.get();

		// logte("DECODE:: %lld %lld", frame->GetPts(), frame->GetPts() * 1000);

		_frame->format = _context->sample_fmt;
		_frame->nb_samples = _context->frame_size;
		_frame->pts = frame->GetPts();
		_frame->pkt_duration = frame->GetDuration();

		_frame->channel_layout = _context->channel_layout;
		_frame->channels = _context->channels;
		_frame->sample_rate = _context->sample_rate;

		if (::av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data");
			break;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			// *result = TranscodeResult::DataError;
			break;
		}

		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));

		int ret = ::avcodec_send_frame(_context, _frame);

		::av_frame_unref(_frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);

			// Failure to send frame to encoder. Wait and put it back in. But it doesn't happen as often as possible.
			_input_buffer.Enqueue(std::move(buffer));
		}

		while (true)
		{
			int ret = ::avcodec_receive_packet(_context, _packet);

			if (ret == AVERROR(EAGAIN))
			{
				// Wait for more packet
				break;
			}
			else if (ret == AVERROR_EOF)
			{
				logte("Error receiving a packet for decoding : AVERROR_EOF");

				// *result = TranscodeResult::DataError;
				// return nullptr;
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for encoding : %d", ret);

				// *result = TranscodeResult::DataError;
				// return nullptr;
				break;
			}
			else
			{
				// Packet is ready
				auto packet_buffer = std::make_shared<MediaPacket>(cmn::MediaType::Audio, 1, _packet->data, _packet->size, _packet->pts, _packet->dts, _packet->duration, MediaPacketFlag::Key);
				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::AAC_ADTS);
				packet_buffer->SetPacketType(cmn::PacketType::RAW);

				// logte("ENCODED:: %lld, %lld, %d, %d", packet_buffer->GetPts(), _packet->pts, _packet->size, _packet->duration);

				::av_packet_unref(_packet);

				// TODO(soulk) : If the pts value are under zero, the dash packettizer does not work.
				if (packet_buffer->GetPts() < 0)
					continue;

				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}

std::shared_ptr<MediaPacket> OvenCodecImplAvcodecEncAAC::RecvBuffer(TranscodeResult *result)
{
	if (!_output_buffer.IsEmpty())
	{
		*result = TranscodeResult::DataReady;

		auto obj = _output_buffer.Dequeue();
		if (obj.has_value())
		{
			return std::move(obj.value());
		}
	}

	*result = TranscodeResult::NoData;

	return nullptr;
}
