//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_hevc.h"

#include <unistd.h>

#include "../transcode_private.h"

OvenCodecImplAvcodecEncHEVC::~OvenCodecImplAvcodecEncHEVC()
{
	Stop();
}

// Notes.
//
// - B-frame must be disabled. because, WEBRTC does not support B-Frame.
//
bool OvenCodecImplAvcodecEncHEVC::Configure(std::shared_ptr<TranscodeContext> context)
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

	_context->framerate = ::av_d2q(_output_context->GetFrameRate(), AV_TIME_BASE);

	_context->bit_rate = _output_context->GetBitrate();
	_context->rc_min_rate = _context->bit_rate;
	_context->rc_max_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate / 2);
	_context->sample_aspect_ratio = (AVRational){1, 1};

	// From avcodec.h:
	// For some codecs, the time base is closer to the field rate than the frame rate.
	// Most notably, H.264 and MPEG-2 specify time_base as half of frame duration
	// if no telecine is used ...
	// Set to time_base ticks per frame. Default 1, e.g., H.264/MPEG-2 set it to 2.
	_context->ticks_per_frame = 2;
	// From avcodec.h:
	// For fixed-fps content, timebase should be 1/framerate and timestamp increments should be identically 1.
	// This often, but not always is the inverse of the frame rate or field rate for video. 1/time_base is not the average frame rate if the frame rate is not constant.

	AVRational codec_timebase = ::av_inv_q(::av_mul_q(::av_d2q(_output_context->GetFrameRate(), AV_TIME_BASE), (AVRational){_context->ticks_per_frame, 1}));
	_context->time_base = codec_timebase;

	// _context->gop_size = _context->framerate.num / _context->framerate.den;
	_context->max_b_frames = 0;
	_context->pix_fmt = AV_PIX_FMT_YUV420P;
	_context->width = _output_context->GetVideoWidth();
	_context->height = _output_context->GetVideoHeight();
	_context->thread_count = 0;

	// 인코딩 품질 및 브라우저 호환성
	// For browser compatibility
	// _context->profile = FF_PROFILE_H264_MAIN;
	_context->profile = FF_PROFILE_HEVC_MAIN;

	// 인코딩 성능
	::av_opt_set(_context->priv_data, "preset", "veryfast", 0);

	// Encoding Delay
	::av_opt_set(_context->priv_data, "tune", "zerolatency", 0);

	// Keyframe Intervasl
	::av_opt_set(_context->priv_data, "x265-params", ov::String::FormatString("pass=1:bframes=0:no-scenecut=1:keyint=%.0f:min-keyint=%.0f:level-idc=4:no-open-gop=1", _output_context->GetFrameRate(), _output_context->GetFrameRate()).CStr(), 0);

	if (::avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec. %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&OvenCodecImplAvcodecEncHEVC::ThreadEncode, this);
		pthread_setname_np(_thread_work.native_handle(), "EncHEVC");
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}

	return true;
}

void OvenCodecImplAvcodecEncHEVC::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();
	_output_buffer.Stop();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("AVC encoder thread has ended.");
	}
}

void OvenCodecImplAvcodecEncHEVC::ThreadEncode()
{
	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
			continue;

		auto frame = std::move(obj.value());

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

		// logte("hevc queue : %d / %lld", _input_buffer.size(), _frame->pts);

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
				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::H265_ANNEXB);
				packet_buffer->SetPacketType(cmn::PacketType::NALU);

				// logte("SendOutputBuffer");
				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}

std::shared_ptr<MediaPacket> OvenCodecImplAvcodecEncHEVC::RecvBuffer(TranscodeResult *result)
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
