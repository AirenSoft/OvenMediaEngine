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
#include "codec/encoder/encoder_avc_xma.h"
#include "codec/encoder/encoder_ffopus.h"
#include "codec/encoder/encoder_hevc_nv.h"
#include "codec/encoder/encoder_hevc_qsv.h"
#include "codec/encoder/encoder_hevc_xma.h"
#include "codec/encoder/encoder_jpeg.h"
#include "codec/encoder/encoder_opus.h"
#include "codec/encoder/encoder_png.h"
#include "codec/encoder/encoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define USE_LEGACY_LIBOPUS false
#define MAX_QUEUE_SIZE 500

TranscodeEncoder::TranscodeEncoder(info::Stream stream_info) : 
	_stream_info(stream_info)
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

	if(_codec_context != nullptr)
	{
		OV_SAFE_FUNC(_codec_context, nullptr, ::avcodec_free_context, &);
	}
	
	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_packet, nullptr, ::av_packet_free, &);
	OV_SAFE_FUNC(_codec_par, nullptr, ::avcodec_parameters_free, &);

	_input_buffer.Clear();
}

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::Create(int32_t encoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> output_track, CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;

	bool use_hwaccel = output_track->GetHardwareAccel();
	auto codec_id = output_track->GetCodecId();
	auto library_id = output_track->GetCodecLibraryId();

	logti("[#%d] hardware acceleration of the encoder is %s. The library to be used will be %s", output_track->GetId(),  use_hwaccel ? "enabled" : "disabled", GetStringFromCodecLibraryId(library_id).CStr());

	switch (codec_id)
	{	
		case cmn::MediaCodecId::H264:

			if (use_hwaccel == true)
			{
				if ( TranscodeGPU::GetInstance()->IsSupportedQSV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::QSV))
				{
					encoder = std::make_shared<EncoderAVCxQSV>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
						goto done;
					}
				}

				if ( TranscodeGPU::GetInstance()->IsSupportedNV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::NVENC))
				{
					encoder = std::make_shared<EncoderAVCxNV>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
						goto done;
					}
				}

				if ( TranscodeGPU::GetInstance()->IsSupportedXMA() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::XMA))
				{
					encoder = std::make_shared<EncoderAVCxXMA>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::XMA);
						goto done;
					}
				}				
			}

			if (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::OPENH264)
			{
				encoder = std::make_shared<EncoderAVCxOpenH264>(info);
				if (encoder != nullptr && encoder->Configure(output_track) == true)
				{

					output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::OPENH264);
					goto done;
				}
			}

			break;
		case cmn::MediaCodecId::H265:
			if (use_hwaccel == true)
			{
				if ( TranscodeGPU::GetInstance()->IsSupportedQSV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::QSV))
				{
					encoder = std::make_shared<EncoderHEVCxQSV>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
						goto done;
					}
				}

				if ( TranscodeGPU::GetInstance()->IsSupportedNV() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::NVENC))
				{
					encoder = std::make_shared<EncoderHEVCxNV>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
						goto done;
					}
				}

				if ( TranscodeGPU::GetInstance()->IsSupportedXMA() == true && (library_id == cmn::MediaCodecLibraryId::AUTO || library_id == cmn::MediaCodecLibraryId::XMA))
				{
					encoder = std::make_shared<EncoderHEVCxXMA>(info);
					if (encoder != nullptr && encoder->Configure(output_track) == true)
					{
						output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::XMA);
						goto done;
					}
				}
			}

			break;
		case cmn::MediaCodecId::Vp8:
			encoder = std::make_shared<EncoderVP8>(info);
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::LIBVPX);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Jpeg:
			encoder = std::make_shared<EncoderJPEG>(info);
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Png:
			encoder = std::make_shared<EncoderPNG>(info);
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Aac:
			encoder = std::make_shared<EncoderAAC>(info);
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				output_track->SetCodecLibraryId(cmn::MediaCodecLibraryId::FDKAAC);
				goto done;
			}

			break;
		case cmn::MediaCodecId::Opus:
#if USE_LEGACY_LIBOPUS
			encoder = std::make_shared<EncoderOPUS>(info);
			if (encoder != nullptr && encoder->Configure(output_track) == true)
			{
				goto done;
			}
#else
			encoder = std::make_shared<EncoderFFOPUS>(info);
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
	_track->SetOriginBitstream(GetBitstreamFormat());

	auto name = ov::String::FormatString("encoder_%s_%d", ::avcodec_get_name(GetCodecID()), _track->GetId());
	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_stream_info.GetApplicationInfo().GetName(),
		_stream_info.GetName(),
		"trs",
		name);
	_input_buffer.SetUrn(urn);
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);

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
