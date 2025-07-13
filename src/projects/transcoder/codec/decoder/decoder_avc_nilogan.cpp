//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "decoder_avc_nilogan.h"

#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/nalu/nal_stream_converter.h>

#include "../../transcoder_gpu.h"
#include "../../transcoder_private.h"
#include "base/info/application.h"

bool DecoderAVCxNILOGAN::InitCodec()
{
	const AVCodec *codec = ::avcodec_find_decoder_by_name("h264_ni_logan_dec");
	if (codec == nullptr)
	{
		logte("Codec not found: %s", cmn::GetCodecIdString(GetCodecID()));
		return false;
	}

	_codec_context = ::avcodec_alloc_context3(codec);
	if (_codec_context == nullptr)
	{
		logte("Could not allocate codec context for %s", cmn::GetCodecIdString(GetCodecID()));
		return false;
	}

	_codec_context->hw_device_ctx = ::av_buffer_ref(TranscodeGPU::GetInstance()->GetDeviceContext(cmn::MediaCodecModuleId::NILOGAN, _track->GetCodecDeviceId()));
	if (_codec_context->hw_device_ctx == nullptr)
	{
		logte("Could not allocate hw device context for %s", cmn::GetCodecIdString(GetCodecID()));
		return false;
	}

	_codec_context->time_base = ffmpeg::compat::TimebaseToAVRational(GetTimebase());
	_codec_context->pkt_timebase = ffmpeg::compat::TimebaseToAVRational(GetTimebase());
	_codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

	auto decoder_config = std::static_pointer_cast<AVCDecoderConfigurationRecord>(GetRefTrack()->GetDecoderConfigurationRecord());

	if (decoder_config->ChromaFormat() > 1)
	{
		logte("Could not initialize codec because nilogan decoder support only AV_PIX_FMT_YUV420P pixel format: %d", decoder_config->ChromaFormat());
		return false;
	}

	if (decoder_config != nullptr)
	{
		_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;	 //Forced here nilogan decoder support only AV_PIX_FMT_YUV420P pixel format
		_codec_context->width = decoder_config->GetWidth();
		_codec_context->height = decoder_config->GetHeight();
	}

	//dec_options
	::av_opt_set(_codec_context->priv_data, "xcoder-params", "out=sw:lowDelayMode=1:lowDelay=100", 0);
	//::av_opt_set(_codec_context->priv_data, "xcoder-params", "out=hw:lowDelayMode=1:lowDelay=100", 0);
	//::av_opt_set(_codec_context->priv_data, "dec", 0, 0);

	if (::avcodec_open2(_codec_context, nullptr, nullptr) < 0)
	{
		logte("Could not open codec: %s", cmn::GetCodecIdString(GetCodecID()));
		return false;
	}

	_parser = ::av_parser_init(ffmpeg::compat::ToAVCodecId(GetCodecID()));
	if (_parser == nullptr)
	{
		logte("Parser not found");
		return false;
	}
	_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

	_change_format = false;

	return true;
}

/*
it seems that dynamic resolution is supported
void DecoderAVCxNILOGAN::UninitCodec()
{
	::avcodec_close(_codec_context);
	::avcodec_free_context(&_codec_context);

	_codec_context = nullptr;
}

bool DecoderAVCxNILOGAN::ReinitCodecIfNeed()
{
	// Netint H.264 decoder does not support dynamic resolution streams. 
	// So, when a resolution change is detected, the codec is reset and recreated.
	if (_codec_context->width != 0 && _codec_context->height != 0 && (_parser->width != _codec_context->width || _parser->height != _codec_context->height))
	{
		logti("Changed input resolution of %u track. (%dx%d -> %dx%d)", GetRefTrack()->GetId(), _codec_context->width, _codec_context->height, _parser->width, _parser->height);

		UninitCodec();

		if (InitCodec() == false)
		{
			return false;
		}
	}

	return true;
}
*/

void DecoderAVCxNILOGAN::CodecThread()
{
	// Initialize the codec and notify the main thread.
	if(_codec_init_event.Submit(InitCodec()) == false)
	{
		return;
	}

	ffmpeg::compat::PadedAlignedBuffer buffer;

	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
		{
			// logte("An error occurred while dequeue : no data");
			continue;
		}

		auto media_packet = std::move(obj.value());

		buffer.CopyFrom(media_packet, media_packet->GetData());

		///////////////////////////////
		// Send to decoder
		///////////////////////////////
		while (buffer.GetRemainedSize() > 0)
		{
			::av_packet_unref(_pkt);

			int parsed_size = ::av_parser_parse2(
				_parser,
				_codec_context,
				&_pkt->data, &_pkt->size,
				buffer.DataAtCurrentOffset(),
				buffer.GetRemainedSize(),
				buffer.GetPts(),
				buffer.GetDts(), 0);
			if (parsed_size < 0)
			{
				logte("An error occurred while parsing: %d", parsed_size);
				break;
			}

			buffer.Advance(parsed_size);

			// If parsed frame is not same as the previous frame, update the codec context.
			// if (ReinitCodecIfNeed() == false)
			// {
			// 	break;
			// }

			if (_pkt->size > 0)
			{
				_pkt->pts	   = _parser->pts;
				_pkt->dts	   = _parser->dts;
				_pkt->flags	   = (_parser->key_frame == 1) ? AV_PKT_FLAG_KEY : 0;
				_pkt->duration = _pkt->dts - _parser->last_dts;

				if (_pkt->duration <= 0LL)
				{
					// It may not be the exact packet duration.
					// However, in general, this method is applied under the assumption that the duration of all packets is similar.
					_pkt->duration = buffer.GetDuration();
				}

				// Keyframe Decode Only
				// If set to decode only key frames, non-keyframe packets are dropped.
				if (GetRefTrack()->IsKeyframeDecodeOnly() == true)
				{
					// Drop non-keyframe packets
					if (!(_pkt->flags & AV_PKT_FLAG_KEY))
					{
						break;
					}
				}

				int ret = ::avcodec_send_packet(_codec_context, _pkt);
				if (ret == AVERROR(EAGAIN))
				{
					// Nothing to do here, just continue
				}
				else if (ret == AVERROR_INVALIDDATA)
				{
					// If a failure occurs due to the absence of a Decoder Configuration, 
					// an Empty frame is created and transmitted. 
					// This is used to replace a failed frame.
					auto empty_frame = std::make_shared<MediaFrame>();
					empty_frame->SetPts(buffer.GetDts());
					empty_frame->SetMediaType(cmn::MediaType::Video);

					Complete(TranscodeResult::NoData, std::move(empty_frame));

					break;
				}
				else if (ret < 0)
				{
					logte("Error error occurred while sending a packet for decoding. reason(%s)", ffmpeg::compat::AVErrorToString(ret).CStr());
					break;
				}
			}
		}

		///////////////////////////////
		// Receive from decoder
		///////////////////////////////
		while (!_kill_flag)
		{
			// Check the decoded frame is available
			int ret = ::avcodec_receive_frame(_codec_context, _frame);
			if (ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for decoding. reason(%s)", ffmpeg::compat::AVErrorToString(ret).CStr());
				continue;
			}
			else
			{
				// Update codec information if needed
				if (_change_format == false)
				{
					auto codec_info = ffmpeg::compat::CodecInfoToString(_codec_context);

					logti("[%s/%s(%u)] input track information: %s",
						  _stream_info.GetApplicationInfo().GetVHostAppName().CStr(), _stream_info.GetName().CStr(), _stream_info.GetId(), codec_info.CStr());
				}

				// If there is no duration, the duration is calculated by framerate and timebase.
				if (_frame->pkt_duration <= 0LL && _codec_context->framerate.num > 0 && _codec_context->framerate.den > 0)
				{
					_frame->pkt_duration = (int64_t)(((double)_codec_context->framerate.den / (double)_codec_context->framerate.num) / (double)GetRefTrack()->GetTimeBase().GetExpr());
				}

				auto decoded_frame = ffmpeg::compat::ToMediaFrame(cmn::MediaType::Video, _frame);
				::av_frame_unref(_frame);
				if (decoded_frame == nullptr)
				{
					continue;
				}

				Complete(!_change_format ? TranscodeResult::FormatChanged : TranscodeResult::DataReady, std::move(decoded_frame));
				_change_format = true;
			}
		}
	}
}
