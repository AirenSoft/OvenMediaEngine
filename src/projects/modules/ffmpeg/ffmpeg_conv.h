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
#include <libswscale/swscale.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/cpu.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
}

#include <base/common_types.h>
#include <base/info/media_track.h>
#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <transcoder/transcoder_context.h>

#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/h265/h265_decoder_configuration_record.h>
#include <modules/bitstream/aac/audio_specific_config.h>
#include <modules/bitstream/opus/opus_specific_config.h>

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
				case AVMEDIA_TYPE_DATA:
					return cmn::MediaType::Data;
				default:
					break;
			}
			return cmn::MediaType::Unknown;
		};

		static enum AVMediaType ToAVMediaType(cmn::MediaType media_type)
		{
			switch (media_type)
			{
				case cmn::MediaType::Video:
					return AVMEDIA_TYPE_VIDEO;
				case cmn::MediaType::Audio:
					return AVMEDIA_TYPE_AUDIO;
				case cmn::MediaType::Data:
					return AVMEDIA_TYPE_DATA;
				default:
					break;
			}
			return AVMEDIA_TYPE_UNKNOWN;
		};

		static cmn::MediaCodecId ToCodecId(enum AVCodecID codec_id)
		{
			switch (codec_id)
			{
				case AV_CODEC_ID_H265:
					return cmn::MediaCodecId::H265;
				case AV_CODEC_ID_H264:
					return cmn::MediaCodecId::H264;
				case AV_CODEC_ID_VP8:
					return cmn::MediaCodecId::Vp8;
				case AV_CODEC_ID_VP9:
					return cmn::MediaCodecId::Vp9;
				case AV_CODEC_ID_FLV1:
					return cmn::MediaCodecId::Flv;
				case AV_CODEC_ID_AAC:
					return cmn::MediaCodecId::Aac;
				case AV_CODEC_ID_MP3:
					return cmn::MediaCodecId::Mp3;
				case AV_CODEC_ID_OPUS:
					return cmn::MediaCodecId::Opus;
				case AV_CODEC_ID_MJPEG:
					return cmn::MediaCodecId::Jpeg;
				case AV_CODEC_ID_PNG:
					return cmn::MediaCodecId::Png;
				default:
					break;
			}

			return cmn::MediaCodecId::None;
		}

		static AVCodecID ToAVCodecId(cmn::MediaCodecId codec_id)
		{
			switch (codec_id)
			{
				case cmn::MediaCodecId::H265:
					return AV_CODEC_ID_H265;
				case cmn::MediaCodecId::H264:
					return AV_CODEC_ID_H264;
				case cmn::MediaCodecId::Vp8:
					return AV_CODEC_ID_VP8;
				case cmn::MediaCodecId::Vp9:
					return AV_CODEC_ID_VP9;
				case cmn::MediaCodecId::Flv:
					return AV_CODEC_ID_FLV1;
				case cmn::MediaCodecId::Aac:
					return AV_CODEC_ID_AAC;
				case cmn::MediaCodecId::Mp3:
					return AV_CODEC_ID_MP3;
				case cmn::MediaCodecId::Opus:
					return AV_CODEC_ID_OPUS;
				case cmn::MediaCodecId::Jpeg:
					return AV_CODEC_ID_MJPEG;
				case cmn::MediaCodecId::Png:
					return AV_CODEC_ID_PNG;
				default:
					break;
			}

			return AV_CODEC_ID_NONE;
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
					sample_fmt = cmn::AudioSample::Format::S16;
					break;
				case AV_SAMPLE_FMT_S32:
					sample_fmt = cmn::AudioSample::Format::S32;
					break;
				case AV_SAMPLE_FMT_FLT:
					sample_fmt = cmn::AudioSample::Format::Flt;
					break;
				case AV_SAMPLE_FMT_DBL:
					sample_fmt = cmn::AudioSample::Format::Dbl;
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

		static int ToAvSampleFormat(cmn::AudioSample::Format format)
		{
			int sample_fmt;
			switch (format)
			{
				case cmn::AudioSample::Format::U8:
					sample_fmt = AV_SAMPLE_FMT_U8;
					break;
				case cmn::AudioSample::Format::S16:
					sample_fmt = AV_SAMPLE_FMT_S16;
					break;
				case cmn::AudioSample::Format::S32:
					sample_fmt = AV_SAMPLE_FMT_S32;
					break;
				case cmn::AudioSample::Format::Flt:
					sample_fmt = AV_SAMPLE_FMT_FLT;
					break;
				case cmn::AudioSample::Format::Dbl:
					sample_fmt = AV_SAMPLE_FMT_DBL;
					break;
				case cmn::AudioSample::Format::U8P:
					sample_fmt = AV_SAMPLE_FMT_U8P;
					break;
				case cmn::AudioSample::Format::S16P:
					sample_fmt = AV_SAMPLE_FMT_S16P;
					break;
				case cmn::AudioSample::Format::S32P:
					sample_fmt = AV_SAMPLE_FMT_S32P;
					break;
				case cmn::AudioSample::Format::FltP:
					sample_fmt = AV_SAMPLE_FMT_FLTP;
					break;
				case cmn::AudioSample::Format::DblP:
					sample_fmt = AV_SAMPLE_FMT_DBLP;
					break;
				default:
					sample_fmt = AV_SAMPLE_FMT_NONE;
					break;
			}

			return sample_fmt;
		}

		static int ToAVChannelLayout(cmn::AudioChannel::Layout channel_layout)
		{
			switch (channel_layout)
			{
				case cmn::AudioChannel::Layout::LayoutMono:
					return AV_CH_LAYOUT_MONO;
				case cmn::AudioChannel::Layout::LayoutStereo:
					return AV_CH_LAYOUT_STEREO;
				case cmn::AudioChannel::Layout::Layout21:
					return AV_CH_LAYOUT_2_1;
				case cmn::AudioChannel::Layout::LayoutSurround:
					return AV_CH_LAYOUT_SURROUND;
				case cmn::AudioChannel::Layout::Layout3Point1:
					return AV_CH_LAYOUT_3POINT1;
				case cmn::AudioChannel::Layout::Layout4Point0:
					return AV_CH_LAYOUT_4POINT0;
				case cmn::AudioChannel::Layout::Layout4Point1:
					return AV_CH_LAYOUT_4POINT1;
				case cmn::AudioChannel::Layout::Layout22:
					return AV_CH_LAYOUT_2_2;
				case cmn::AudioChannel::Layout::Layout5Point0:
					return AV_CH_LAYOUT_QUAD;
				case cmn::AudioChannel::Layout::Layout5Point1:
					return AV_CH_LAYOUT_5POINT1;
				case cmn::AudioChannel::Layout::Layout5Point1Back:
					return AV_CH_LAYOUT_5POINT1_BACK;
				case cmn::AudioChannel::Layout::Layout6Point0:
					return AV_CH_LAYOUT_6POINT0;
				case cmn::AudioChannel::Layout::Layout6Point0Front:
					return AV_CH_LAYOUT_6POINT0_FRONT;
				case cmn::AudioChannel::Layout::LayoutHexagonal:
					return AV_CH_LAYOUT_HEXAGONAL;
				case cmn::AudioChannel::Layout::Layout6Point1:
					return AV_CH_LAYOUT_6POINT1;
				case cmn::AudioChannel::Layout::Layout6Point1Back:
					return AV_CH_LAYOUT_6POINT1_BACK;
				case cmn::AudioChannel::Layout::Layout6Point1Front:
					return AV_CH_LAYOUT_6POINT1_FRONT;
				case cmn::AudioChannel::Layout::Layout7Point0:
					return AV_CH_LAYOUT_7POINT0;
				case cmn::AudioChannel::Layout::Layout7Point0Front:
					return AV_CH_LAYOUT_7POINT0_FRONT;
				case cmn::AudioChannel::Layout::Layout7Point1:
					return AV_CH_LAYOUT_7POINT1;
				case cmn::AudioChannel::Layout::Layout7Point1Wide:
					return AV_CH_LAYOUT_7POINT1_WIDE;
				case cmn::AudioChannel::Layout::Layout7Point1WideBack:
					return AV_CH_LAYOUT_7POINT1_WIDE_BACK;
				case cmn::AudioChannel::Layout::LayoutOctagonal:
					return AV_CH_LAYOUT_OCTAGONAL;
				default:
					break;
			}

			return AV_CH_LAYOUT_MONO;
		}

		static cmn::AudioChannel::Layout ToAudioChannelLayout(int channel_layout)
		{
			switch (channel_layout)
			{
				case AV_CH_LAYOUT_MONO:
					return cmn::AudioChannel::Layout::LayoutMono;
				case AV_CH_LAYOUT_STEREO:
					return cmn::AudioChannel::Layout::LayoutStereo;
				case AV_CH_LAYOUT_2_1:
					return cmn::AudioChannel::Layout::Layout21;
				case AV_CH_LAYOUT_SURROUND:
					return cmn::AudioChannel::Layout::LayoutSurround;
				case AV_CH_LAYOUT_3POINT1:
					return cmn::AudioChannel::Layout::Layout3Point1;
				case AV_CH_LAYOUT_4POINT0:
					return cmn::AudioChannel::Layout::Layout4Point0;
				case AV_CH_LAYOUT_4POINT1:
					return cmn::AudioChannel::Layout::Layout4Point1;
				case AV_CH_LAYOUT_2_2:
					return cmn::AudioChannel::Layout::Layout22;
				case AV_CH_LAYOUT_QUAD:
					return cmn::AudioChannel::Layout::Layout5Point0;
				case AV_CH_LAYOUT_5POINT1:
					return cmn::AudioChannel::Layout::Layout5Point1;
				case AV_CH_LAYOUT_5POINT1_BACK:
					return cmn::AudioChannel::Layout::Layout5Point1Back;
				case AV_CH_LAYOUT_6POINT0:
					return cmn::AudioChannel::Layout::Layout6Point0;
				case AV_CH_LAYOUT_6POINT0_FRONT:
					return cmn::AudioChannel::Layout::Layout6Point0Front;
				case AV_CH_LAYOUT_HEXAGONAL:
					return cmn::AudioChannel::Layout::LayoutHexagonal;
				case AV_CH_LAYOUT_6POINT1:
					return cmn::AudioChannel::Layout::Layout6Point1;
				case AV_CH_LAYOUT_6POINT1_BACK:
					return cmn::AudioChannel::Layout::Layout6Point1Back;
				case AV_CH_LAYOUT_6POINT1_FRONT:
					return cmn::AudioChannel::Layout::Layout6Point1Front;
				case AV_CH_LAYOUT_7POINT0:
					return cmn::AudioChannel::Layout::Layout7Point0;
				case AV_CH_LAYOUT_7POINT0_FRONT:
					return cmn::AudioChannel::Layout::Layout7Point0Front;
				case AV_CH_LAYOUT_7POINT1:
					return cmn::AudioChannel::Layout::Layout7Point1;
				case AV_CH_LAYOUT_7POINT1_WIDE:
					return cmn::AudioChannel::Layout::Layout7Point1Wide;
				case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
					return cmn::AudioChannel::Layout::Layout7Point1WideBack;
				case AV_CH_LAYOUT_OCTAGONAL:
					return cmn::AudioChannel::Layout::LayoutOctagonal;
			}

			return cmn::AudioChannel::Layout::LayoutUnknown;
		}

		static std::shared_ptr<MediaTrack> CreateMediaTrack(AVStream *stream)
		{
			auto media_track = std::make_shared<MediaTrack>();
			if (ToMediaTrack(stream, media_track) == false)
			{
				return nullptr;
			}

			return media_track;
		}

		static bool ToMediaTrack(AVStream* stream, std::shared_ptr<MediaTrack> media_track)
		{
			media_track->SetId(stream->index);
			media_track->SetMediaType(ffmpeg::Conv::ToMediaType(stream->codecpar->codec_type));
			media_track->SetCodecId(ffmpeg::Conv::ToCodecId(stream->codecpar->codec_id));
			media_track->SetTimeBase(stream->time_base.num, stream->time_base.den);
			media_track->SetBitrateByConfig(stream->codecpar->bit_rate);
			media_track->SetStartFrameTime(0);
			media_track->SetLastFrameTime(0);

			if (media_track->GetMediaType() == cmn::MediaType::Unknown || media_track->GetCodecId() == cmn::MediaCodecId::None)
			{
				return false;
			}

			switch (media_track->GetMediaType())
			{
				case cmn::MediaType::Video:
					media_track->SetFrameRateByConfig(av_q2d(stream->r_frame_rate));
					media_track->SetWidth(stream->codecpar->width);
					media_track->SetHeight(stream->codecpar->height);
					break;
				case cmn::MediaType::Audio:
					media_track->SetSampleRate(stream->codecpar->sample_rate);
					media_track->GetSample().SetFormat(ffmpeg::Conv::ToAudioSampleFormat(stream->codecpar->format));
					media_track->GetChannel().SetLayout(ffmpeg::Conv::ToAudioChannelLayout(stream->codecpar->channel_layout));
					break;
				default:
					break;
			}

			switch (media_track->GetCodecId())
			{
				case cmn::MediaCodecId::H264:
				{
					if (stream->codecpar->extradata_size > 0)
					{
						// AVCC format
						auto avc_config = std::make_shared<AVCDecoderConfigurationRecord>();
						auto extra_data = std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size, true);

						if (avc_config->Parse(extra_data) == false)
						{
							return false;
						}

						media_track->SetDecoderConfigurationRecord(avc_config);
					}

					
					break;
				}
				case cmn::MediaCodecId::H265:
				{
					if (stream->codecpar->extradata_size > 0)
					{
						// HVCC format
						auto hevc_config = std::make_shared<HEVCDecoderConfigurationRecord>();
						auto extra_data = std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size, true);

						if (hevc_config->Parse(extra_data) == false)
						{
							return false;
						}

						media_track->SetDecoderConfigurationRecord(hevc_config);
					}

					break;
				}
				case cmn::MediaCodecId::Aac:
				{
					if (stream->codecpar->extradata_size > 0)
					{
						// ASC format
						auto asc = std::make_shared<AudioSpecificConfig>();
						auto extra_data = std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size, true);

						if (asc->Parse(extra_data) == false)
						{
							return false;
						}

						media_track->SetDecoderConfigurationRecord(asc);
					}

					break;
				}
				case cmn::MediaCodecId::Opus:
				{
					if (stream->codecpar->extradata_size > 0)
					{
						// ASC format
						auto opus_config = std::make_shared<OpusSpecificConfig>();
						auto extra_data = std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size, true);

						if (opus_config->Parse(extra_data) == false)
						{
							return false;
						}

						media_track->SetDecoderConfigurationRecord(opus_config);
					}

					break;
				}				case cmn::MediaCodecId::Vp8:
				case cmn::MediaCodecId::Vp9:
				case cmn::MediaCodecId::Flv:
				case cmn::MediaCodecId::Mp3:
				case cmn::MediaCodecId::Jpeg:
				case cmn::MediaCodecId::Png:
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
					media_frame->GetChannels().SetLayout(ffmpeg::Conv::ToAudioChannelLayout(frame->channel_layout));
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

		static inline int64_t GetDurationPerFrame(cmn::MediaType media_type, std::shared_ptr<MediaTrack>& context, AVFrame* frame = nullptr)
		{
			switch (media_type)
			{
				case cmn::MediaType::Video: {
					// Calculate duration using framerate in timebase
					int den = context->GetTimeBase().GetDen();

					// TODO(Keukhan) : If there is no framerate value, the frame rate value cannot be calculated normally.
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

		static AVRational TimebaseToAVRational(const cmn::Timebase& timebase)
		{
			return (AVRational){
				.num = timebase.GetNum(),
				.den = timebase.GetDen()};
		}

		static ov::String CodecInfoToString(const AVCodecContext* context, const AVCodecParameters* parameters)
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
						// frame_size: 1234
						message.AppendFormat(", frame_size: %d", parameters->frame_size);

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
						message.AppendFormat("%d Hz, %s(%d), %s, ", parameters->sample_rate, channel_layout, parameters->channels, ::av_get_sample_fmt_name(static_cast<AVSampleFormat>(parameters->format)));
					}

					message.AppendFormat("%d kbps, ", (parameters->bit_rate / 1024));
					// timebase: 1/48000
					message.AppendFormat("timebase: %d/%d, ", context->time_base.num, context->time_base.den);

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

		static ov::String AVErrorToString(int32_t error)
		{
			char error_buffer[AV_ERROR_MAX_STRING_SIZE]{};
			::av_strerror(error, error_buffer, OV_COUNTOF(error_buffer));

			ov::String message = error_buffer;

			return error_buffer;
		}

		static bool ToAVStream(std::shared_ptr<MediaTrack> media_track, AVStream* av_stream)
		{
			if (media_track == nullptr || av_stream == nullptr)
			{
				return false;
			}

			av_stream->start_time = 0;
			av_stream->time_base = AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()};
			AVCodecParameters* codecpar = av_stream->codecpar;
			codecpar->codec_type 		= ToAVMediaType(media_track->GetMediaType());
			codecpar->codec_id 			= ToAVCodecId(media_track->GetCodecId());
			codecpar->codec_tag 		= 0;
			codecpar->bit_rate 			= media_track->GetBitrate();

			// Set Decoder Configuration Record to extradata
			if (media_track->GetDecoderConfigurationRecord() != nullptr && 
				media_track->GetDecoderConfigurationRecord()->GetData() != nullptr && 
				media_track->GetDecoderConfigurationRecord()->GetData()->GetLength() > 0)
			{
				codecpar->extradata_size 	= media_track->GetDecoderConfigurationRecord()->GetData()->GetLength();
				codecpar->extradata 		= (uint8_t*)av_malloc(codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memset(codecpar->extradata, 0, codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memcpy(codecpar->extradata, media_track->GetDecoderConfigurationRecord()->GetData()->GetDataAs<uint8_t>(), codecpar->extradata_size);
			}

			switch (media_track->GetMediaType())
			{
				case cmn::MediaType::Video: {
					av_stream->r_frame_rate 		= AVRational{ (int)round(media_track->GetFrameRate() * 1000), 1000};
					av_stream->avg_frame_rate 		= AVRational{ (int)round(media_track->GetFrameRate() * 1000), 1000};
					av_stream->sample_aspect_ratio 	= AVRational{1, 1};
					codecpar->width 				= media_track->GetWidth();
					codecpar->height 				= media_track->GetHeight();
					codecpar->sample_aspect_ratio 	= AVRational{1, 1};
				}
				break;

				case cmn::MediaType::Audio: {
					codecpar->channels 				= static_cast<int>(media_track->GetChannel().GetCounts());
					codecpar->channel_layout 		= ToAVChannelLayout(media_track->GetChannel().GetLayout());
					codecpar->sample_rate 			= media_track->GetSample().GetRateNum();
					codecpar->frame_size 			= (media_track->GetAudioSamplesPerFrame()!=0)?media_track->GetAudioSamplesPerFrame():1024;
				}
				break;

				default:
					return false;
			}

			return true;
		}

		static ov::String GetCodecName(AVCodecID codec_id)
		{
			return ov::String::FormatString("%s", ::avcodec_get_name(codec_id));
		}

		static ov::String GetFormatByExtension(ov::String extension, ov::String default_format)
		{
			if (extension == "mp4")
			{
				return "mp4";
			}
			else if (extension == "ts")
			{
				return "mpegts";
			}
			else if (extension == "webm")
			{
				return "webm";
			}

			return default_format;
		}

		static bool IsSupportCodec(ov::String format, cmn::MediaCodecId codec_id)
		{
			if (format == "mp4")
			{
				if (codec_id == cmn::MediaCodecId::H264 ||
					codec_id == cmn::MediaCodecId::H265 ||
					codec_id == cmn::MediaCodecId::Aac ||
					codec_id == cmn::MediaCodecId::Mp3)
				{
					return true;
				}
			}
			else if (format == "mpegts")
			{
				if (codec_id == cmn::MediaCodecId::H264 ||
					codec_id == cmn::MediaCodecId::H265 ||
					codec_id == cmn::MediaCodecId::Vp8 ||
					codec_id == cmn::MediaCodecId::Vp9 ||
					codec_id == cmn::MediaCodecId::Aac ||
					codec_id == cmn::MediaCodecId::Mp3 ||
					codec_id == cmn::MediaCodecId::Opus)
				{
					return true;
				}
			}
			else if (format == "webm")
			{
				if (
					codec_id == cmn::MediaCodecId::Vp8 ||
					codec_id == cmn::MediaCodecId::Vp9 ||
					codec_id == cmn::MediaCodecId::Opus)
				{
					return true;
				}
			}

			return false;
		}

		static bool SetHwDeviceCtxOfAVCodecContext(AVCodecContext* context, AVBufferRef* hw_device_ctx)
		{
			context->hw_device_ctx = ::av_buffer_ref(hw_device_ctx);
			if (context->hw_device_ctx == nullptr)
			{
				return false;
			}

			return true;
		}

		static bool SetHWFramesCtxOfAVCodecContext(AVCodecContext* context)
		{
			AVBufferRef* hw_frames_ref;
			int err = 0;
			if (!(hw_frames_ref = ::av_hwframe_ctx_alloc(context->hw_device_ctx)))
			{
				return false;
			}

			auto constraints = ::av_hwdevice_get_hwframe_constraints(context->hw_device_ctx, nullptr);
			if(constraints == nullptr)
			{
				return false;
			}

			AVHWFramesContext* frames_ctx = nullptr;
			frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
			frames_ctx->format = *(constraints->valid_hw_formats);
			frames_ctx->sw_format = *(constraints->valid_sw_formats);;
			frames_ctx->width = context->width;
			frames_ctx->height = context->height;
			frames_ctx->initial_pool_size = 10;
			
			if ((err = ::av_hwframe_ctx_init(hw_frames_ref)) < 0)
			{
				::av_buffer_unref(&hw_frames_ref);
				return false;
			}

			context->hw_frames_ctx = ::av_buffer_ref(hw_frames_ref);
			
			if (!context->hw_frames_ctx)
				err = AVERROR(ENOMEM);
			
			::av_buffer_unref(&hw_frames_ref);

			return true;
		}

		static bool SetHwDeviceCtxOfAVFilterContext(AVFilterContext* context, AVBufferRef* hw_device_ctx)
		{
			context->hw_device_ctx = ::av_buffer_ref(hw_device_ctx);
			if (context->hw_device_ctx == nullptr)
			{
				return false;
			}

			return true;
		}
		static bool SetHWFramesCtxOfAVFilterLink(AVFilterLink* context, AVBufferRef* hw_device_ctx, int32_t width, int32_t height)
		{
			AVBufferRef* hw_frames_ref;
			AVHWFramesContext* frames_ctx = NULL;

			if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx)))
			{
				return false;
			}

			auto constraints = av_hwdevice_get_hwframe_constraints(hw_device_ctx, nullptr);

			frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
			frames_ctx->format = *(constraints->valid_hw_formats);
			frames_ctx->sw_format = *(constraints->valid_sw_formats);
			frames_ctx->width = width;
			frames_ctx->height = height;
			frames_ctx->initial_pool_size = 10;

			if (av_hwframe_ctx_init(hw_frames_ref) < 0)
			{
				av_buffer_unref(&hw_frames_ref);
				return false;
			}

			context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);

			av_buffer_unref(&hw_frames_ref);

			return true;
		}
	};

}  // namespace ffmpeg
