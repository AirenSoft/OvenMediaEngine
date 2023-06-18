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
#include "codec/decoder/decoder_avc_xma.h"
#include "codec/decoder/decoder_hevc.h"
#include "codec/decoder/decoder_hevc_nv.h"
#include "codec/decoder/decoder_hevc_qsv.h"
#include "codec/decoder/decoder_hevc_xma.h"
#include "codec/decoder/decoder_opus.h"
#include "codec/decoder/decoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 500

std::shared_ptr<TranscodeDecoder> TranscodeDecoder::Create(int32_t decoder_id, const info::Stream &info, std::shared_ptr<MediaTrack> track, CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeDecoder> decoder = nullptr;

	bool use_hwaccel = track->GetHardwareAccel();
	logtd("Hardware acceleration of the decoder is %s", use_hwaccel ? "enabled" : "disabled");

	switch (track->GetCodecId())
	{
		case cmn::MediaCodecId::H264:
			if(use_hwaccel == true)
			{
				if (TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
				{
					decoder = std::make_shared<DecoderAVCxQSV>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
						goto done;
					}
				}

				if (TranscodeGPU::GetInstance()->IsSupportedNV() == true)
				{
					decoder = std::make_shared<DecoderAVCxNV>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
						goto done;
					}
				}

				if (TranscodeGPU::GetInstance()->IsSupportedXMA() == true)
				{
					decoder = std::make_shared<DecoderAVCxXMA>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::XMA);
						goto done;
					}
				}
			}

			decoder = std::make_shared<DecoderAVC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}
			break;

		case cmn::MediaCodecId::H265:

			if(use_hwaccel == true)
			{
				if (TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
				{
					decoder = std::make_shared<DecoderHEVCxQSV>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::QSV);
						goto done;
					}
				}

				if (TranscodeGPU::GetInstance()->IsSupportedNV() == true)
				{
					decoder = std::make_shared<DecoderHEVCxNV>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::NVENC);
						goto done;
					}
				}

				if (TranscodeGPU::GetInstance()->IsSupportedXMA() == true)
				{
					decoder = std::make_shared<DecoderHEVCxXMA>(info);
					if (decoder != nullptr && decoder->Configure(track) == true)
					{
						track->SetCodecLibraryId(cmn::MediaCodecLibraryId::XMA);
						goto done;
					}
				}
			}

			decoder = std::make_shared<DecoderHEVC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}
			break;

		case cmn::MediaCodecId::Vp8:
			decoder = std::make_shared<DecoderVP8>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}
			break;

		case cmn::MediaCodecId::Aac:
			decoder = std::make_shared<DecoderAAC>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
				goto done;
			}
			break;

		case cmn::MediaCodecId::Opus:
			decoder = std::make_shared<DecoderOPUS>(info);
			if (decoder != nullptr && decoder->Configure(track) == true)
			{
				track->SetCodecLibraryId(cmn::MediaCodecLibraryId::DEFAULT);
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
	auto urn = info::ManagedQueue::URN(_stream_info.GetApplicationName(), _stream_info.GetName(), "trs", ov::String::FormatString("decoder_%s_%d", ::avcodec_get_name(GetCodecID()), track->GetId()));

	_input_buffer.SetUrn(urn.CStr());
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