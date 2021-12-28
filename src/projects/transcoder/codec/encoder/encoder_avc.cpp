//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_avc.h"

#include <unistd.h>

#include "../../transcoder_private.h"

#define USE_OPENH264			1

EncoderAVC::~EncoderAVC()
{
	Stop();
}

bool EncoderAVC::SetCodecParams()
{
#if USE_OPENH264
	_codec_context->framerate = ::av_d2q((_encoder_context->GetFrameRate() > 0) ? _encoder_context->GetFrameRate() : _encoder_context->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->bit_rate = _codec_context->rc_min_rate = _codec_context->rc_max_rate = _encoder_context->GetBitrate();
	_codec_context->rc_buffer_size = static_cast<int>(_codec_context->bit_rate / 2);
	_codec_context->sample_aspect_ratio = ::av_make_q(1, 1);
	_codec_context->time_base = ::av_inv_q(::av_mul_q(_codec_context->framerate, (AVRational){_codec_context->ticks_per_frame, 1}));
	_codec_context->gop_size = _codec_context->framerate.num / _codec_context->framerate.den;
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetPixelFormat();
	_codec_context->width = _encoder_context->GetVideoWidth();
	_codec_context->height = _encoder_context->GetVideoHeight();
	_codec_context->thread_count = FFMAX(4, av_cpu_count() / 3);
	_codec_context->slices = _codec_context->thread_count;

	::av_opt_set(_codec_context->priv_data, "rc_mode", "timestamp", 0);
	::av_opt_set(_codec_context->priv_data, "allow_skip_frames", "true", 0);

	::av_opt_set(_codec_context->priv_data, "profile", "constrained_baseline", 0);
	::av_opt_set(_codec_context->priv_data, "coder", "cavlc", 0);


	if (_encoder_context->GetPreset() == "slower" || _encoder_context->GetPreset() == "slow")
	{
		_codec_context->qmin = 10;
		_codec_context->qmax = 41;
	}
	else if (_encoder_context->GetPreset() == "fast" || _encoder_context->GetPreset() == "faster")
	{
		_codec_context->qmin = 25;
		_codec_context->qmax = 51;		
	}
	else // else if (_encoder_context->GetPreset() == "medium")
	{
		_codec_context->qmin = 10;
		_codec_context->qmax = 51;
	}
#else
	_codec_context->framerate = ::av_d2q((_encoder_context->GetFrameRate() > 0) ? _encoder_context->GetFrameRate() : _encoder_context->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->bit_rate = _codec_context->rc_min_rate = _codec_context->rc_max_rate = _encoder_context->GetBitrate();
	_codec_context->rc_buffer_size = static_cast<int>(_codec_context->bit_rate / 2);
	_codec_context->sample_aspect_ratio = ::av_make_q(1, 1);

	// From avcodec.h:
	// For some codecs, the time base is closer to the field rate than the frame rate.
	// Most notably, H.264 and MPEG-2 specify time_base as half of frame duration
	// if no telecine is used ...
	// Set to time_base ticks per frame. Default 1, e.g., H.264/MPEG-2 set it to 2.
	_codec_context->ticks_per_frame = 2;

	// From avcodec.h:
	// For fixed-fps content, timebase should be 1/framerate and timestamp increments should be identically 1.
	// This often, but not always is the inverse of the frame rate or field rate for video. 1/time_base is not the average frame rate if the frame rate is not constant.
	_codec_context->time_base = ::av_inv_q(::av_mul_q(_codec_context->framerate, (AVRational){_codec_context->ticks_per_frame, 1}));
	_codec_context->gop_size = _codec_context->framerate.num / _codec_context->framerate.den;
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetPixelFormat();
	_codec_context->width = _encoder_context->GetVideoWidth();
	_codec_context->height = _encoder_context->GetVideoHeight();
	_codec_context->thread_count = FFMAX(4, av_cpu_count() / 3);

	// Preset
	if (_encoder_context->GetPreset() == "slower")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "slower", 0);
	}
	else if (_encoder_context->GetPreset() == "slow")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "slow", 0);
	}
	else if (_encoder_context->GetPreset() == "medium")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "medium", 0);
	}
	else if (_encoder_context->GetPreset() == "fast")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "fast", 0);
	}
	else if (_encoder_context->GetPreset() == "faster")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "faster", 0);
	}
	else
	{
		// Default
		::av_opt_set(_codec_context->priv_data, "preset", "faster", 0);
	}

	// Profile
	::av_opt_set(_codec_context->priv_data, "profile", "baseline", 0);

	// Tune
	::av_opt_set(_codec_context->priv_data, "tune", "zerolatency", 0);

	// Remove the sliced-thread option from encoding delay. Browser compatibility in MAC environment
	::av_opt_set(_codec_context->priv_data, "x264opts", "bframes=0:sliced-threads=0:b-adapt=1:no-scenecut:keyint=30:min-keyint=30", 0);
#endif

	return true;
}

// Notes.
//
// - B-frame must be disabled. because, WEBRTC does not support B-Frame.
//
bool EncoderAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

#if USE_OPENH264	
	AVCodec *codec = ::avcodec_find_encoder_by_name("libopenh264");
#else
	AVCodec *codec = ::avcodec_find_encoder_by_name("libx264");	
#endif
	if (codec == nullptr)
	{
		logte("Could not find encoder: %d (%s)", codec_id, ::avcodec_get_name(codec_id));
		return false;
	}

	_codec_context = ::avcodec_alloc_context3(codec);
	if (_codec_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	if (SetCodecParams() == false)
	{
		logte("Could not set codec parameters for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	if (::avcodec_open2(_codec_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&EncoderAVC::ThreadEncode, this);
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

void EncoderAVC::Stop()
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

void EncoderAVC::ThreadEncode()
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

		if (::av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			break;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			break;
		}

		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		::memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		::memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		int ret = ::avcodec_send_frame(_codec_context, _frame);
		::av_frame_unref(_frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		///////////////////////////////////////////////////
		// The encoded packet is taken from the codec.
		///////////////////////////////////////////////////
		while (true)
		{
			// Check frame is availble
			int ret = ::avcodec_receive_packet(_codec_context, _packet);

			if (ret == AVERROR(EAGAIN))
			{
				// More packets are needed for encoding.
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
					0,
					cmn::MediaType::Video,
					0,
					_packet->data,
					_packet->size,
					_packet->pts,
					_packet->dts,
					-1L,
					(_packet->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

				if (packet_buffer == nullptr)
				{
					logte("Could not allocate the media packet");
					break;
				}

				packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::H264_ANNEXB);
				packet_buffer->SetPacketType(cmn::PacketType::NALU);

				::av_packet_unref(_packet);

				SendOutputBuffer(std::move(packet_buffer));
			}
		}
	}
}

std::shared_ptr<MediaPacket> EncoderAVC::RecvBuffer(TranscodeResult *result)
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
