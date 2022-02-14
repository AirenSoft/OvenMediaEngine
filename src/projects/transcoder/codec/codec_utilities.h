//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "../transcoder_private.h"
#include "codec_base.h"

class TranscoderUtilities
{
public:
	static inline std::shared_ptr<MediaFrame> AvFrameToMediaFrame(cmn::MediaType media_type, AVFrame* frame)
	{
		switch (media_type)
		{
			case cmn::MediaType::Video: {
				auto media_frame = std::make_shared<MediaFrame>();

				media_frame->SetMediaType(media_type);
				media_frame->SetWidth(frame->width);
				media_frame->SetHeight(frame->height);
				media_frame->SetFormat(frame->format);
				media_frame->SetPts((frame->pts == AV_NOPTS_VALUE) ? -1LL : frame->pts);
				media_frame->SetDuration(frame->pkt_duration);

				AVFrame* moved_frame = av_frame_alloc();
				av_frame_move_ref(moved_frame, frame);
				media_frame->SetPrivData(moved_frame);

				return media_frame;
			}
			break;
			case cmn::MediaType::Audio: {
				auto media_frame = std::make_shared<MediaFrame>();

				media_frame->SetMediaType(media_type);
				media_frame->SetBytesPerSample(::av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format)));
				media_frame->SetNbSamples(frame->nb_samples);
				media_frame->SetChannelCount(frame->channels);
				media_frame->SetSampleRate(frame->sample_rate);
				media_frame->SetFormat(frame->format);
				media_frame->SetDuration(frame->pkt_duration);
				media_frame->SetPts((frame->pts == AV_NOPTS_VALUE) ? -1LL : frame->pts);

				AVFrame* moved_frame = av_frame_alloc();
				av_frame_move_ref(moved_frame, frame);
				media_frame->SetPrivData(moved_frame);

				return media_frame;
			}
			break;
			default:
				return nullptr;
				break;
		}

		return nullptr;
	}

	static inline int64_t GetDurationPerFrame(cmn::MediaType media_type, std::shared_ptr<TranscodeContext>& context, AVFrame* frame = nullptr)
	{
		switch (media_type)
		{
			case cmn::MediaType::Video: {
				// Calculate duration using framerate in timebase
				int den = context->GetTimeBase().GetDen();

				// TODO(soulk) : If there is no framerate value, the frame rate value cannot be calculated normally.
				int64_t duration = (den == 0) ? 0LL : (float)den / context->GetFrameRate();
				return duration;
			}
			break;
			case cmn::MediaType::Audio:
			default: {
				float frame_duration_in_second = frame->nb_samples * (1.0f / frame->sample_rate);
				int frame_duration_in_timebase = static_cast<int>(frame_duration_in_second * context->GetTimeBase().GetDen());
				return frame_duration_in_timebase;
			}
			break;
		}

		return -1;
	}

	static bool IsPlanar(AVSampleFormat format)
	{
		switch (format)
		{
			case AV_SAMPLE_FMT_U8:
			case AV_SAMPLE_FMT_S16:
			case AV_SAMPLE_FMT_S32:
			case AV_SAMPLE_FMT_FLT:
			case AV_SAMPLE_FMT_DBL:
			case AV_SAMPLE_FMT_S64:
				return false;

			case AV_SAMPLE_FMT_U8P:
			case AV_SAMPLE_FMT_S16P:
			case AV_SAMPLE_FMT_S32P:
			case AV_SAMPLE_FMT_FLTP:
			case AV_SAMPLE_FMT_DBLP:
			case AV_SAMPLE_FMT_S64P:
				return true;

			default:
				return false;
		}
	}

	static AVFrame* MediaFrameToAVFrame(cmn::MediaType media_type, std::shared_ptr<const MediaFrame> src)
	{
		if (src->GetPrivData() != nullptr)
		{
			return static_cast<AVFrame*>(src->GetPrivData());
		}

		return nullptr;
	}

	static std::shared_ptr<MediaPacket> AvPacketToMediaPacket(AVPacket* src, cmn::MediaType media_type, cmn::BitstreamFormat format, cmn::PacketType packet_type)
	{
		auto packet_buffer = std::make_shared<MediaPacket>(
			0,
			media_type,
			0,
			src->data,
			src->size,
			src->pts,
			src->dts,
			src->duration,
			(src->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

		if (packet_buffer == nullptr)
		{
			return nullptr;
		}

		packet_buffer->SetBitstreamFormat(format);
		packet_buffer->SetPacketType(packet_type);

		return packet_buffer;
	}
};
