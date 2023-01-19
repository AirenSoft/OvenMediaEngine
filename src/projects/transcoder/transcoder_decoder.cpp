//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_decoder.h"

#include "codec/decoder/decoder_aac.h"
#include "codec/decoder/decoder_avc.h"
#include "codec/decoder/decoder_avc_nv.h"
#include "codec/decoder/decoder_avc_qsv.h"
#include "codec/decoder/decoder_hevc.h"
#include "codec/decoder/decoder_hevc_nv.h"
#include "codec/decoder/decoder_hevc_qsv.h"
#include "codec/decoder/decoder_opus.h"
#include "codec/decoder/decoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 120

std::shared_ptr<TranscodeDecoder> TranscodeDecoder::Create(int32_t decoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> track, CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeDecoder> decoder = nullptr;

	bool use_hwaccel = track->GetHardwareAccel();
	logtd("Hardware acceleration of the decoder is %s", use_hwaccel ? "enabled" : "disabled");

	switch (track->GetCodecId())
	{
		case cmn::MediaCodecId::H264:
			if ((use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
			{
				decoder = std::make_shared<DecoderAVCxQSV>(info);
				if (decoder != nullptr && decoder->Configure(track) == true)
				{
					goto done;
				}
			}

			if ((use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedNV() == true)
			{
				decoder = std::make_shared<DecoderAVCxNV>(info);
				if (decoder != nullptr && decoder->Configure(track) == true)
				{
					goto done;
				}
			}

			decoder = std::make_shared<DecoderAVC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				goto done;
			}
			break;

		case cmn::MediaCodecId::H265:

			if ((use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
			{
				decoder = std::make_shared<DecoderHEVCxQSV>(info);
				if (decoder != nullptr && decoder->Configure(track) == true)
				{
					goto done;
				}
			}

			if ((use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedNV() == true)
			{
				decoder = std::make_shared<DecoderHEVCxNV>(info);
				if (decoder != nullptr && decoder->Configure(track) == true)
				{
					goto done;
				}
			}

			decoder = std::make_shared<DecoderHEVC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				goto done;
			}
			break;
		case cmn::MediaCodecId::Vp8:
			decoder = std::make_shared<DecoderVP8>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				goto done;
			}
			break;
		case cmn::MediaCodecId::Aac:
			decoder = std::make_shared<DecoderAAC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				goto done;
			}
			break;
		case cmn::MediaCodecId::Opus:
			decoder = std::make_shared<DecoderOPUS>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				goto done;
			}
			break;
		default:
			OV_ASSERT(false, "Not supported codec: %d", track->GetCodecId());
			break;
	}

done:
	if (decoder != nullptr)
	{
		decoder->SetDecoderId(decoder_id);
		decoder->SetCompleteHandler(complete_handler);
	}

	return decoder;
}

TranscodeDecoder::TranscodeDecoder(info::Stream stream_info)
	: _stream_info(stream_info)
{
	_pkt = ::av_packet_alloc();
	_frame = ::av_frame_alloc();
	_codec_par = avcodec_parameters_alloc();
}

TranscodeDecoder::~TranscodeDecoder()
{
	if (_context != nullptr && _context->codec != nullptr)
	{
		if (_context->codec->capabilities & AV_CODEC_CAP_ENCODER_FLUSH)
		{
			::avcodec_flush_buffers(_context);
		}
	}

	if (_context != nullptr)
	{
		::avcodec_free_context(&_context);
	}

	if (_codec_par != nullptr)
	{
		::avcodec_parameters_free(&_codec_par);
	}

	if (_frame != nullptr)
	{
		::av_frame_free(&_frame);
	}

	if (_pkt != nullptr)
	{
		::av_packet_free(&_pkt);
	}

	if (_parser != nullptr)
	{
		::av_parser_close(_parser);
	}

	_input_buffer.Clear();
}

std::shared_ptr<MediaTrack> &TranscodeDecoder::GetRefTrack()
{
	return _track;
}

cmn::Timebase TranscodeDecoder::GetTimebase()
{
	return GetRefTrack()->GetTimeBase();
}

void TranscodeDecoder::SetDecoderId(int32_t decoder_id)
{
	_decoder_id = decoder_id;
}

bool TranscodeDecoder::Configure(std::shared_ptr<MediaTrack> track)
{
	_input_buffer.SetAlias(ov::String::FormatString("Input queue of Decoder. track(%d) codec(%s/%d)", track->GetId(), ::avcodec_get_name(GetCodecID()), GetCodecID()));
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);

	_track = track;

	return (_track != nullptr);
}

void TranscodeDecoder::SendBuffer(std::shared_ptr<const MediaPacket> packet)
{
	_input_buffer.Enqueue(std::move(packet));
}

void TranscodeDecoder::SendOutputBuffer(TranscodeResult result, std::shared_ptr<MediaFrame> frame)
{
	// Invoke callback function when encoding/decoding is completed.
	if (_complete_handler)
	{
		frame->SetTrackId(_decoder_id);
		_complete_handler(result, _decoder_id, std::move(frame));
	}
}

void TranscodeDecoder::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();

	if (_codec_thread.joinable())
	{
		_codec_thread.join();

		logtd(ov::String::FormatString("decoder %s thread has ended", avcodec_get_name(GetCodecID())).CStr());
	}
}

const ov::String TranscodeDecoder::ShowCodecParameters(const AVCodecContext *context, const AVCodecParameters *parameters)
{
	ov::String message;

	message.AppendFormat("[%s] ", ::av_get_media_type_string(parameters->codec_type));

	switch (parameters->codec_type)
	{
		case AVMEDIA_TYPE_UNKNOWN:
			message = "Unknown media type";
			break;

		case AVMEDIA_TYPE_VIDEO:
		case AVMEDIA_TYPE_AUDIO:
			// H.264 (Baseline
			message.AppendFormat("%s (%s", ::avcodec_get_name(parameters->codec_id), ::avcodec_profile_name(parameters->codec_id, parameters->profile));

			if (parameters->level >= 0)
			{
				// lv: 5.2
				message.AppendFormat(" %.1f", parameters->level / 10.0f);
			}
			message.Append(')');

			if (parameters->codec_tag != 0)
			{
				char tag[AV_FOURCC_MAX_STRING_SIZE]{};
				::av_fourcc_make_string(tag, parameters->codec_tag);

				// (avc1 / 0x31637661
				message.AppendFormat(", (%s / 0x%08X", tag, parameters->codec_tag);

				if (parameters->extradata_size != 0)
				{
					// extra: 1234
					message.AppendFormat(", extra: %d", parameters->extradata_size);
				}

				message.Append(')');
			}

			message.Append(", ");

			if (parameters->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				int gcd = ::av_gcd(parameters->width, parameters->height);

				if (gcd == 0)
				{
					OV_ASSERT2(false);
					gcd = 1;
				}

				int digit = 0;

				if (context->framerate.den > 1)
				{
					digit = 3;
				}

				// yuv420p, 1920x1080 [SAR 1:1 DAR 16:9], 24 fps
				message.AppendFormat("%s, %dx%d [SAR %d:%d DAR %d:%d], %.*f fps, ",
									 ::av_get_pix_fmt_name(static_cast<AVPixelFormat>(parameters->format)),
									 parameters->width, parameters->height,
									 parameters->sample_aspect_ratio.num, parameters->sample_aspect_ratio.den,
									 parameters->width / gcd, parameters->height / gcd,
									 digit, ::av_q2d(context->framerate));
			}
			else
			{
				char channel_layout[16]{};
				::av_get_channel_layout_string(channel_layout, OV_COUNTOF(channel_layout), parameters->channels, parameters->channel_layout);

				// 48000 Hz, stereo, fltp,
				message.AppendFormat("%d Hz, %s, %s, ", parameters->sample_rate, channel_layout, ::av_get_sample_fmt_name(static_cast<AVSampleFormat>(parameters->format)));
			}

			message.AppendFormat("%d kbps, ", (parameters->bit_rate / 1024));
			// timebase: 1/48000
			message.AppendFormat("timebase: %d/%d, ", context->time_base.num, context->time_base.den);
			// frame_size: 1234
			message.AppendFormat("frame_size: %d", parameters->frame_size);
			if (parameters->block_align != 0)
			{
				// align: 32
				message.AppendFormat(", align: %d", parameters->block_align);
			}

			break;

		case AVMEDIA_TYPE_DATA:
			message = "Data";
			break;

		case AVMEDIA_TYPE_SUBTITLE:
			message = "Subtitle";
			break;

		case AVMEDIA_TYPE_ATTACHMENT:
			message = "Attachment";
			break;

		case AVMEDIA_TYPE_NB:
			message = "NB";
			break;
	}

	return message;
}