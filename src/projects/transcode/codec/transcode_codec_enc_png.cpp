//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_png.h"

#include<fstream>


#include <unistd.h>

#define OV_LOG_TAG "TranscodeCodec"


OvenCodecImplAvcodecEncPng::~OvenCodecImplAvcodecEncPng()
{
	Stop();
}

bool OvenCodecImplAvcodecEncPng::Configure(std::shared_ptr<TranscodeContext> context)
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
	_context->codec_type = AVMEDIA_TYPE_VIDEO;
	_context->time_base = codec_timebase;
	_context->pix_fmt = AV_PIX_FMT_RGBA;
	_context->width = _output_context->GetVideoWidth();
	_context->height = _output_context->GetVideoHeight();
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

		_thread_work = std::thread(&OvenCodecImplAvcodecEncPng::ThreadEncode, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}

	return true;
}

void OvenCodecImplAvcodecEncPng::Stop()
{
	_kill_flag = true;

	_queue_event.Notify();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("Png encoder thread has ended.");
	}
}

void OvenCodecImplAvcodecEncPng::ThreadEncode()
{
	while(!_kill_flag)
	{
		_queue_event.Wait();

		std::unique_lock<std::mutex> mlock(_mutex);

		if (_input_buffer.empty())
		{
			continue;
		}

		auto frame = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		mlock.unlock();

		///////////////////////////////////////////////////
		// Request frame encoding to codec
		///////////////////////////////////////////////////

		_frame->format = frame->GetFormat();
		_frame->pts = frame->GetPts();
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

		// Convert to Frame->Format -> Context->Format
		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		::memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		::memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		int ret = ::avcodec_send_frame(_context, _frame);

		::av_frame_unref(_frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);

			// Failure to send frame to encoder. Wait and put it back in. But it doesn't happen as often as possible.
			std::unique_lock<std::mutex> mlock(_mutex);
			_input_buffer.push_front(std::move(frame));
			mlock.unlock();
			_queue_event.Notify();
		}

		///////////////////////////////////////////////////
		// The encoded packet is taken from the codec.
		///////////////////////////////////////////////////
		while(true)
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
				logte("encoded size(png) : %d", _packet->size);

				std::ofstream writeFile; 
				writeFile.open("test.png");

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
				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::PNG);
				packet_buffer->SetPacketType(cmn::PacketType::RAW);

				::av_packet_unref(_packet);

				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}


std::shared_ptr<MediaPacket> OvenCodecImplAvcodecEncPng::RecvBuffer(TranscodeResult *result)
{
	std::unique_lock<std::mutex> mlock(_mutex);
	if(!_output_buffer.empty())
	{
		*result = TranscodeResult::DataReady;

		auto packet = std::move(_output_buffer.front());
		_output_buffer.pop_front();

		return std::move(packet);
	}

	*result = TranscodeResult::NoData;

	return nullptr;

}
