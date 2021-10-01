//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_ffopus.h"

#include "../../transcoder_private.h"

EncoderFFOPUS::~EncoderFFOPUS()
{
	Stop();
}

bool EncoderFFOPUS::Configure(std::shared_ptr<TranscodeContext> output_context)
{
	if (TranscodeEncoder::Configure(output_context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();
	AVCodec *codec = ::avcodec_find_encoder(codec_id);
	if (codec == nullptr)
	{
		logte("Codec not found: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	_context = ::avcodec_alloc_context3(codec);
	if (_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	_context->bit_rate = output_context->GetBitrate();
	_context->sample_fmt = AV_SAMPLE_FMT_S16;
	_context->sample_rate = output_context->GetAudioSampleRate();
	_context->channel_layout = static_cast<uint64_t>(output_context->GetAudioChannel().GetLayout());
	_context->channels = output_context->GetAudioChannel().GetCounts();
	_context->codec = codec;
	_context->codec_id = codec_id;
	_context->cutoff = 12000;  // SuperWideBand
	_context->compression_level = 10;
	::av_opt_set(_context->priv_data, "application", "lowdelay", 0);
	::av_opt_set(_context->priv_data, "frame_duration", "20.0", 0);
	::av_opt_set(_context->priv_data, "packet_loss", "10", 0);
	::av_opt_set(_context->priv_data, "vbr", "off", 0);

	if (::avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	output_context->SetAudioSamplesPerFrame(_context->frame_size);


	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&EncoderFFOPUS::ThreadEncode, this);
		pthread_setname_np(_thread_work.native_handle(), ov::String::FormatString("Enc%s", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start encoder thread.");
		_kill_flag = true;

		return false;
	}

	return true;
}

void EncoderFFOPUS::Stop()
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

void EncoderFFOPUS::ThreadEncode()
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

		_frame->format = _context->sample_fmt;
		_frame->nb_samples = _context->frame_size;
		_frame->pts = frame->GetPts();
		_frame->pkt_duration = frame->GetDuration();
		_frame->channel_layout = _context->channel_layout;
		_frame->channels = _context->channels;
		_frame->sample_rate = _context->sample_rate;

		if (::av_frame_get_buffer(_frame, true) < 0)
		{
			logte("Could not allocate the audio frame data");
			break;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			break;
		}

		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		int ret = ::avcodec_send_frame(_context, _frame);
		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		::av_frame_unref(_frame);

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
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for encoding : %d", ret);
				break;
			}
			else
			{
				auto packet_buffer = std::make_shared<MediaPacket>(cmn::MediaType::Audio, 1, _packet->data, _packet->size, _packet->pts, _packet->dts, _packet->duration, MediaPacketFlag::Key);
				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::OPUS);
				packet_buffer->SetPacketType(cmn::PacketType::RAW);

				::av_packet_unref(_packet);

				// TODO(soulk) : If the pts value are under zero, the dash packettizer does not work.
				if (packet_buffer->GetPts() < 0)
					continue;

				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}

std::shared_ptr<MediaPacket> EncoderFFOPUS::RecvBuffer(TranscodeResult *result)
{
	if (!_output_buffer.IsEmpty())
	{
		*result = TranscodeResult::DataReady;

		auto obj = _output_buffer.Dequeue();
		if (obj.has_value())
		{
			return obj.value();
		}
	}

	*result = TranscodeResult::NoData;

	return nullptr;
}
