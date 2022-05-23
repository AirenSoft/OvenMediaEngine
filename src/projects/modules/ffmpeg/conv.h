//==============================================================================
//
//  Type Convert Utilities
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}
#include <base/common_types.h>
#include <base/info/media_track.h>
#include <base/ovlibrary/ovlibrary.h>
#include <transcoder/transcoder_context.h>


namespace ffmpeg
{
	class Conv
	{
	public:
		static cmn::MediaType ToMediaType(enum AVMediaType codec_type)
		{
			switch (codec_type)
			{
				case AVMEDIA_TYPE_VIDEO:
					return cmn::MediaType::Video;
				case AVMEDIA_TYPE_AUDIO:
					return cmn::MediaType::Audio;
				default:
					break;
			}
			return cmn::MediaType::Unknown;
		};

		static cmn::MediaCodecId ToCodecId(enum AVCodecID codec_id)
		{
			switch (codec_id)
			{
				case AV_CODEC_ID_H264:
					return cmn::MediaCodecId::H264;
				case AV_CODEC_ID_VP8:
					return cmn::MediaCodecId::Vp8;
				case AV_CODEC_ID_FLV1:
					return cmn::MediaCodecId::Flv;
				case AV_CODEC_ID_AAC:
					return cmn::MediaCodecId::Aac;
				case AV_CODEC_ID_MP3:
					return cmn::MediaCodecId::Mp3;
				case AV_CODEC_ID_OPUS:
					return cmn::MediaCodecId::Opus;
				default:
					break;
			}

			return cmn::MediaCodecId::None;
		}

		static cmn::AudioSample::Format ToAudioSampleFormat(int format)
		{
			cmn::AudioSample::Format sample_fmt;
			switch (format)
			{
				case AV_SAMPLE_FMT_U8:
					sample_fmt = cmn::AudioSample::Format::U8;
					break;
				case AV_SAMPLE_FMT_S16:
					sample_fmt = cmn::AudioSample::Format::S16P;
					break;
				case AV_SAMPLE_FMT_S32:
					sample_fmt = cmn::AudioSample::Format::S16P;
					break;
				case AV_SAMPLE_FMT_FLT:
					sample_fmt = cmn::AudioSample::Format::S16P;
					break;
				case AV_SAMPLE_FMT_DBL:
					sample_fmt = cmn::AudioSample::Format::S16P;
					break;
				case AV_SAMPLE_FMT_U8P:
					sample_fmt = cmn::AudioSample::Format::U8P;
					break;
				case AV_SAMPLE_FMT_S16P:
					sample_fmt = cmn::AudioSample::Format::S16P;
					break;
				case AV_SAMPLE_FMT_S32P:
					sample_fmt = cmn::AudioSample::Format::S32P;
					break;
				case AV_SAMPLE_FMT_FLTP:
					sample_fmt = cmn::AudioSample::Format::FltP;
					break;
				case AV_SAMPLE_FMT_DBLP:
					sample_fmt = cmn::AudioSample::Format::DblP;
					break;
				default:
					sample_fmt = cmn::AudioSample::Format::None;
					break;
			}

			return sample_fmt;
		}

		static cmn::AudioChannel::Layout ToAudioChannelLayout(int channels)
		{
			cmn::AudioChannel::Layout channel_layout;
			switch (channels)
			{
				case 1:
					channel_layout = cmn::AudioChannel::Layout::LayoutMono;
					break;
				case 2:
					channel_layout = cmn::AudioChannel::Layout::LayoutStereo;
					break;
				default:
					channel_layout = cmn::AudioChannel::Layout::LayoutUnknown;
					break;
			}

			return channel_layout;
		}

		static bool ToMediaTrack(AVStream* stream, std::shared_ptr<MediaTrack> media_track)
		{
			media_track->SetId(stream->index);
			media_track->SetMediaType(ffmpeg::Conv::ToMediaType(stream->codecpar->codec_type));
			media_track->SetCodecId(ffmpeg::Conv::ToCodecId(stream->codecpar->codec_id));
			media_track->SetTimeBase(stream->time_base.num, stream->time_base.den);
			media_track->SetBitrate(stream->codecpar->bit_rate);
			media_track->SetStartFrameTime(0);
			media_track->SetLastFrameTime(0);

			if (media_track->GetMediaType() == cmn::MediaType::Unknown || media_track->GetCodecId() == cmn::MediaCodecId::None)
			{
				return false;
			}

			switch (media_track->GetMediaType())
			{
				case cmn::MediaType::Video:
					media_track->SetFrameRate(av_q2d(stream->r_frame_rate));
					media_track->SetWidth(stream->codecpar->width);
					media_track->SetHeight(stream->codecpar->height);
					break;
				case cmn::MediaType::Audio:
					media_track->SetSampleRate(stream->codecpar->sample_rate);
					media_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(stream->codecpar->format));
					media_track->GetChannel().SetLayout(ffmpeg::Conv::ToAudioChannelLayout(stream->codecpar->channels));
					break;
				default:
					break;
			}

			return true;
		}

		static inline std::shared_ptr<MediaFrame> ToMediaFrame(cmn::MediaType media_type, AVFrame* frame)
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

		static AVFrame* ToAVFrame(cmn::MediaType media_type, std::shared_ptr<const MediaFrame> src)
		{
			if (src->GetPrivData() != nullptr)
			{
				return static_cast<AVFrame*>(src->GetPrivData());
			}

			return nullptr;
		}

		static std::shared_ptr<MediaPacket> ToMediaPacket(AVPacket* src, cmn::MediaType media_type, cmn::BitstreamFormat format, cmn::PacketType packet_type)
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

		static std::shared_ptr<MediaPacket> ToMediaPacket(uint32_t msid, int32_t track_id, AVPacket* src, cmn::MediaType media_type, cmn::BitstreamFormat format, cmn::PacketType packet_type)
		{
			auto packet_buffer = std::make_shared<MediaPacket>(
				msid,
				media_type,
				track_id,
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

}  // namespace ffmpeg