//==============================================================================
//
//  Transcoder
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_encoder.h"

#include <utility>

#include "codec/encoder/encoder_aac.h"
#include "codec/encoder/encoder_avc_nv.h"
#include "codec/encoder/encoder_avc_openh264.h"
#include "codec/encoder/encoder_avc_qsv.h"
#include "codec/encoder/encoder_ffopus.h"
#include "codec/encoder/encoder_hevc_nv.h"
#include "codec/encoder/encoder_hevc_qsv.h"
#include "codec/encoder/encoder_jpeg.h"
#include "codec/encoder/encoder_opus.h"
#include "codec/encoder/encoder_png.h"
#include "codec/encoder/encoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define USE_LEGACY_LIBOPUS false
#define MAX_QUEUE_SIZE 120

TranscodeEncoder::TranscodeEncoder()
{
	_packet = ::av_packet_alloc();
	_frame = ::av_frame_alloc();
	_codec_par = ::avcodec_parameters_alloc();
}

TranscodeEncoder::~TranscodeEncoder()
{
	Stop();

	if (_codec_context != nullptr && _codec_context->codec != nullptr)
	{
		if (_codec_context->codec->capabilities & AV_CODEC_CAP_ENCODER_FLUSH)
		{
			::avcodec_flush_buffers(_codec_context);
		}
	}

	OV_SAFE_FUNC(_codec_context, nullptr, ::avcodec_free_context, &);
	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_packet, nullptr, ::av_packet_free, &);
	OV_SAFE_FUNC(_codec_par, nullptr, ::avcodec_parameters_free, &);

	_input_buffer.Clear();
}

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::Create(int32_t encoder_id, std::shared_ptr<MediaTrack> output_track, CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;

	bool use_hwaccel = output_track->GetHardwareAccel();
	auto codec_id = output_track->GetCodecId();
	auto library_id = output_track->GetCodecLibraryId();

	logti("[#%d] hwardware acceleration of the encoder is %s. The library to be used will be %s", output_track->GetId(),  use_hwaccel ? "enabled" : "disabled", GetStringFromCodecLibraryId(library_id).CStr());

	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:

			if ( (use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedQSV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::QSV))
			{
				encoder = std::make_shared<EncoderAVCxQSV>();
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{
					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
					goto done;
				}
			}

			if ( (use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedNV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::NVENC))
			{
				encoder = std::make_shared<EncoderAVCxNV>();
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{
					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
					goto done;
				}
			}

			if (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::OPENH264)
			{
				encoder = std::make_shared<EncoderAVCxOpenH264>();
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{
					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::OPENH264);
					goto done;
				}
			}

			break;
		case cmn::MediaCodecId::H265:
			if ( (use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedQSV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::QSV))
			{
				encoder = std::make_shared<EncoderHEVCxQSV>();
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{
					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
					goto done;
				}
			}

			if ( (use_hwaccel == true) && TranscodeGPU::GetInstance()->IsSupportedNV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::NVENC))
			{
				encoder = std::make_shared<EncoderHEVCxNV>();
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{
					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
					goto done;
				}
			}

			break;
		case cmn::MediaCodecId::Vp8:
			encoder = std::make_shared<EncoderVP8>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::LIBVPX);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Jpeg:
			encoder = std::make_shared<EncoderJPEG>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				goto done;
			}

			break;
		case cmn::MediaCodecId::Png:
			encoder = std::make_shared<EncoderPNG>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				goto done;
			}

			break;
		case cmn::MediaCodecId::Aac:
			encoder = std::make_shared<EncoderAAC>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::FDKAAC);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Opus:
#if USE_LEGACY_LIBOPUS
			encoder = std::make_shared<EncoderOPUS>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				goto done;
			}
#else
			encoder = std::make_shared<EncoderFFOPUS>();
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::LIBOPUS);
				goto done;
			}
#endif
			break;
		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

done:
	if (encoder)
	{
		encoder->SetEncoderId(encoder_id);
		encoder->SetCompleteHandler(complete_handler);
	}
	
	return encoder;
}

cmn::Timebase TranscodeEncoder::GetTimebase() const
{
	return _track->GetTimeBase();
}

void TranscodeEncoder::SetEncoderId(int32_t encoder_id)
{
	_encoder_id = encoder_id;
}

bool TranscodeEncoder::Configure(std::shared_ptr<MediaTrack> output_track)
{
	_track = output_track;

	_input_buffer.SetAlias(ov::String::FormatString("Input queue of Encoder. track(%d) codec(%s/%d)", output_track->GetId(), ::avcodec_get_name(GetCodecID()), GetCodecID()));
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);
	_track->SetOriginBitstream(GetBitstreamFormat());

	return (_track != nullptr);
}

std::shared_ptr<MediaTrack> &TranscodeEncoder::GetRefTrack()
{
	return _track;
}

void TranscodeEncoder::SendBuffer(std::shared_ptr<const MediaFrame> frame)
{
	_input_buffer.Enqueue(std::move(frame));
}

void TranscodeEncoder::SendOutputBuffer(std::shared_ptr<MediaPacket> packet)
{
	if (_complete_handler)
	{
		_complete_handler(_encoder_id, std::move(packet));
	}
}

void TranscodeEncoder::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();

	if (_codec_thread.joinable())
	{
		_codec_thread.join();
		logtd(ov::String::FormatString("encoder %s thread has ended", avcodec_get_name(GetCodecID())).CStr());
	}
}
