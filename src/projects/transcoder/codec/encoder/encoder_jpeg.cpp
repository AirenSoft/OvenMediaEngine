//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_jpeg.h"

#include <unistd.h>

#include <fstream>

#include "../../transcoder_private.h"

EncoderJPEG::~EncoderJPEG()
{
	Stop();
}

bool EncoderJPEG::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

	AVCodec *codec = ::avcodec_find_encoder(codec_id);

	if (codec == nullptr)
	{
		logte("Could not find encoder: %d (%s)", codec_id, ::avcodec_get_name(codec_id));
		return false;
	}

	_context = ::avcodec_alloc_context3(codec);

	if (_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	AVRational codec_timebase = ::av_inv_q(::av_mul_q(::av_d2q(_output_context->GetFrameRate(), AV_TIME_BASE), (AVRational){_context->ticks_per_frame, 1}));
	_context->time_base = codec_timebase;
	_context->pix_fmt =  (AVPixelFormat)GetPixelFormat();
	_context->width = _output_context->GetVideoWidth();
	_context->height = _output_context->GetVideoHeight();
	_context->flags = AV_CODEC_FLAG_QSCALE;
	_context->global_quality = _context->qmin * FF_QP2LAMBDA;
	_context->framerate = ::av_d2q(_output_context->GetFrameRate(), AV_TIME_BASE);

	if (::avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&EncoderJPEG::ThreadEncode, this);
		pthread_setname_np(_thread_work.native_handle(),  ov::String::FormatString("Enc%s", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start encoder thread.");
		_kill_flag = true;

		return false;
	}

	return true;
}

void EncoderJPEG::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();
	_output_buffer.Stop();
	
	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("Jpeg encoder thread has ended.");
	}
}

void EncoderJPEG::ThreadEncode()
{
	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
			continue;

		auto frame = std::move(obj.value());

		// If the MJPEG encoding performance is insufficient, drop the pending frame.
		while (_input_buffer.Size() >= 2)
		{
			_input_buffer.Dequeue();
		}

		///////////////////////////////////////////////////
		// Request frame encoding to codec
		///////////////////////////////////////////////////

		_frame->format = frame->GetFormat();
		_frame->nb_samples = 1;
		_frame->pts = frame->GetPts();
		// The encoder will not pass this duration
		_frame->pkt_duration = frame->GetDuration();

		_frame->width = frame->GetWidth();
		_frame->height = frame->GetHeight();
		_frame->linesize[0] = frame->GetStride(0);
		_frame->linesize[1] = frame->GetStride(1);
		_frame->linesize[2] = frame->GetStride(2);

		if (::av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			// *result = TranscodeResult::DataError;
			break;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			// *result = TranscodeResult::DataError;
			break;
		}

		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		::memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		::memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		int ret = ::avcodec_send_frame(_context, _frame);
		// int ret = 0;
		::av_frame_unref(_frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);

			// Failure to send frame to encoder. Wait and put it back in. But it doesn't happen as often as possible.
			// _input_buffer.Enqueue(std::move(frame));
		}

		///////////////////////////////////////////////////
		// The encoded packet is taken from the codec.
		///////////////////////////////////////////////////
		while (true)
		{
			// Check frame is availble
			int ret = ::avcodec_receive_packet(_context, _packet);

			if (ret == AVERROR(EAGAIN))
			{
				// More packets are needed for encoding.

				// logte("Error receiving a packet for decoding : EAGAIN");

				break;
			}
			else if (ret == AVERROR_EOF)
			{
				logte("Error receiving a packet for decoding : AVERROR_EOF");
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
#if 0
				logte("encoded size(jpeg) : %d", _packet->size);

				std::ofstream writeFile; 
				writeFile.open("test.jpg");

				if (writeFile.is_open())   
				{
					writeFile.write((const char*)_packet->data, _packet->size);    
				}
				writeFile.close();
#endif

				// Encoded packet is ready
				auto packet_buffer = std::make_shared<MediaPacket>(
					cmn::MediaType::Video,
					0,
					_packet->data,
					_packet->size,
					_packet->pts,
					_packet->dts,
					-1L,
					(_packet->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);
				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::JPEG);
				packet_buffer->SetPacketType(cmn::PacketType::RAW);

				::av_packet_unref(_packet);

				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}

std::shared_ptr<MediaPacket> EncoderJPEG::RecvBuffer(TranscodeResult *result)
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
