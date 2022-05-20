//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "file_stream.h"

#include <base/info/application.h>
#include <base/ovlibrary/byte_io.h>
#include <modules/rtp_rtcp/rtp_depacketizer_mpeg4_generic_audio.h>

#include "file_private.h"
#include "file_provider.h"

namespace pvd
{
	std::shared_ptr<FileStream> FileStream::Create(const std::shared_ptr<pvd::PullApplication> &application,
												   const uint32_t stream_id, 
												   const ov::String &stream_name,
												   const std::vector<ov::String> &url_list, 
												   std::shared_ptr<pvd::PullStreamProperties> properties)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::File);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<FileStream>(application, stream_info, url_list, properties);
		if (!stream->PullStream::Start())
		{
			// Explicit deletion
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	FileStream::FileStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list, std::shared_ptr<pvd::PullStreamProperties> properties)
		: pvd::PullStream(application, stream_info, url_list, properties)
	{
		SetState(State::IDLE);
	}

	FileStream::~FileStream()
	{
		PullStream::Stop();
		Release();
	}

	std::shared_ptr<pvd::FileProvider> FileStream::GetFileProvider()
	{
		return std::static_pointer_cast<FileProvider>(_application->GetParentProvider());
	}

	void FileStream::Release()
	{
	}

	bool FileStream::StartStream(const std::shared_ptr<const ov::Url> &url)
	{
		// Only start from IDLE, ERROR, STOPPED
		if (!(GetState() == State::IDLE || GetState() == State::ERROR || GetState() == State::STOPPED))
		{
			return true;
		}

		_url = url;
		
		ov::StopWatch stop_watch;

		stop_watch.Start();
		if (ConnectTo() == false)
		{
			return false;
		}
		_origin_request_time_msec = stop_watch.Elapsed();

		stop_watch.Update();

		if (RequestDescribe() == false)
		{
			Release();
			return false;
		}

		if (RequestPlay() == false)
		{
			Release();
			return false;
		}

		_sent_sequence_header = false;
		_origin_response_time_msec = stop_watch.Elapsed();

		// Stream was created completly
		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(PullStream::GetSharedPtr()));
		if (_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginConnectionTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginSubscribeTimeMSec(_origin_response_time_msec);
		}

		return true;
	}

	bool FileStream::RestartStream(const std::shared_ptr<const ov::Url> &url)
	{
		logti("[%s/%s(%u)] stream tries to reconnect to %s", GetApplicationTypeName(), GetName().CStr(), GetId(), url->ToUrlString().CStr());
		return StartStream(url);
	}

	bool FileStream::StopStream()
	{
		if (GetState() == State::STOPPED)
		{
			return true;
		}

		if (!RequestStop())
		{
			// Force terminate
			SetState(State::ERROR);
		}

		Release();

		return true;
	}

	std::shared_ptr<AVFormatContext> FileStream::CreateFormatContext()
	{
		AVFormatContext *format_context = nullptr;
		::avformat_alloc_output_context2(&format_context, nullptr, nullptr, nullptr);
		if (format_context == nullptr)
		{
			logte("Could not create format context");
			return nullptr;
		}

		return std::shared_ptr<AVFormatContext>(format_context, [](AVFormatContext *format_context) {
			if (format_context != nullptr)
			{
				::avformat_free_context(format_context);
			}
		});
	}

	bool FileStream::ConnectTo()
	{
		if (GetState() == State::PLAYING || GetState() == State::TERMINATED)
		{
			return false;
		}

		int err = 0;

		auto url = ov::String::FormatString("%s%s", GetApplicationInfo().GetConfig().GetProviders().GetFileProvider().GetRootPath().CStr(), _url->Path().CStr());

		_format_context = nullptr;
		logtw("Trying to open file: %s", url.CStr());
		if ((err = ::avformat_open_input(&_format_context, url.CStr(), nullptr, nullptr)) < 0)
		{
			SetState(State::ERROR);

			char errbuf[256];
			av_strerror(err, errbuf, sizeof(errbuf));

			logte("Failed to connect to RTMP server.(%s/%s) : %s", GetApplicationInfo().GetName().CStr(), GetName().CStr(), errbuf);

			return false;
		}

		SetState(State::CONNECTED);

		return true;
	}

	bool FileStream::RequestDescribe()
	{
		if (GetState() != State::CONNECTED)
		{
			return false;
		}

		if (::avformat_find_stream_info(_format_context, NULL) < 0)
		{
			SetState(State::ERROR);
			logte("Could not find stream information");

			return false;
		}

		for (uint32_t track_id = 0; track_id < _format_context->nb_streams; track_id++)
		{
			AVStream *stream = _format_context->streams[track_id];

			logtd("[%d] media_type[%d] codec_id[%d], extradata_size[%d] tb[%d/%d]", track_id, stream->codecpar->codec_type, stream->codecpar->codec_id, stream->codecpar->extradata_size, stream->time_base.num, stream->time_base.den);

			auto new_track = pvd::FileStream::AvStreamToMediaTrack(stream);
			if (!new_track)
			{
				continue;
			}

			AddTrack(new_track);
		}

		SetState(State::DESCRIBED);

		return true;
	}

	bool FileStream::RequestPlay()
	{
		if (GetState() != State::DESCRIBED)
		{
			return false;
		}

		SetState(State::PLAYING);

		_play_request_time.Start();

		return true;
	}

	std::shared_ptr<MediaPacket> FileStream::AvPacketToMediaPacket(AVPacket *src, cmn::MediaType media_type, cmn::BitstreamFormat format, cmn::PacketType packet_type)
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

	void FileStream::SendSequenceHeader()
	{
		if (_sent_sequence_header)
			return;

		for (uint32_t track_id = 0; track_id < _format_context->nb_streams; ++track_id)
		{
			AVStream *stream = _format_context->streams[track_id];

			auto track = GetTrack(stream->index);
			if (track == nullptr)
			{
				continue;
			}

			auto media_type = track->GetMediaType();
			auto codec_id = track->GetCodecId();

			if (codec_id == cmn::MediaCodecId::H264)
			{
				// @extratata == AVCDecoderConfigurationRecord
				auto media_packet = std::make_shared<MediaPacket>(
					GetMsid(),
					media_type,
					track->GetId(),
					std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size),
					0,
					0,
					cmn::BitstreamFormat::H264_AVCC,
					cmn::PacketType::SEQUENCE_HEADER);

				SendFrame(media_packet);
			}
			else if (codec_id == cmn::MediaCodecId::Aac)
			{
				// @extratata == AACSpecificConfig
				auto media_packet = std::make_shared<MediaPacket>(
					GetMsid(),
					media_type,
					track->GetId(),
					std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size),
					0,
					0,
					cmn::BitstreamFormat::AAC_RAW,
					cmn::PacketType::SEQUENCE_HEADER);

				SendFrame(media_packet);
			}
		}

		_sent_sequence_header = true;
	}

	bool FileStream::RequestStop()
	{
		if (GetState() != State::PLAYING)
		{
			return false;
		}

		avformat_close_input(&_format_context);
		_format_context = nullptr;

		return true;
	}

	PullStream::ProcessMediaResult FileStream::ProcessMediaPacket()
	{
		// auto format_context = _format_context.get();
		if (_format_context == nullptr)
		{
			return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		AVPacket packet;
		packet.data = nullptr;
		packet.size = 0;

		while (true)
		{
			int32_t ret = ::av_read_frame(_format_context, &packet);
			if (ret < 0)
			{
				// This is a feature added to the existing ffmpeg for OME.
				if (ret == AVERROR(EAGAIN))
				{
					return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
				}
				else if ((ret == AVERROR_EOF || ::avio_feof(_format_context->pb)))
				{
					// If EOF is not receiving packets anymore, end thread.
					logti("%s/%s(%u) FileStream has finished.", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
					SetState(State::STOPPED);
					return ProcessMediaResult::PROCESS_MEDIA_FINISH;
				}

				// If the connection is broken, terminate the thread.
				logte("%s/%s(%u) FileStream's I/O has broken.", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
				SetState(State::ERROR);
				return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
			}

			// Get Track
			auto track = GetTrack(packet.stream_index);
			if (track == nullptr)
			{
				::av_packet_unref(&packet);

				return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
			}

			cmn::BitstreamFormat bitstream_format = cmn::BitstreamFormat::Unknown;
			cmn::PacketType packet_type = cmn::PacketType::Unknown;
			switch (track->GetCodecId())
			{
				case cmn::MediaCodecId::H264:
					bitstream_format = cmn::BitstreamFormat::H264_AVCC;
					packet_type = cmn::PacketType::NALU;
					if (packet.flags == 1)
					{
						SendSequenceHeader();
					}
					break;
				case cmn::MediaCodecId::Aac:
					bitstream_format = cmn::BitstreamFormat::AAC_ADTS;
					packet_type = cmn::PacketType::RAW;
				default:
					::av_packet_unref(&packet);
					continue;
			}

			auto media_packet = AvPacketToMediaPacket(&packet, track->GetMediaType(), bitstream_format, packet_type);
			media_packet->SetMsid(GetMsid());
			::av_packet_unref(&packet);

			int64_t pkt_ms = static_cast<int64_t>(static_cast<double>(media_packet->GetPts()) * track->GetTimeBase().GetExpr() * 1000);

			auto pts = AdjustTimestampByBase(media_packet->GetTrackId(), media_packet->GetPts(), std::numeric_limits<int64_t>::max());
			media_packet->SetPts(pts);
			// media_packet->SetDts(dts); // Keep the dts

			SendFrame(media_packet);

			if (_play_request_time.Elapsed() < pkt_ms)
			{
				break;
			}
		}

		return ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}

	std::shared_ptr<MediaTrack> FileStream::AvStreamToMediaTrack(AVStream *stream)
	{
		// AVStream to MediaTrack TODO
		auto new_track = std::make_shared<MediaTrack>();

		cmn::MediaType media_type;
		switch (stream->codecpar->codec_type)
		{
			case AVMEDIA_TYPE_VIDEO:
				media_type = cmn::MediaType::Video;
				break;
			case AVMEDIA_TYPE_AUDIO:
				media_type = cmn::MediaType::Audio;
				break;
			default:
				media_type = cmn::MediaType::Unknown;
				break;
		}

		cmn::MediaCodecId media_codec;
		switch (stream->codecpar->codec_id)
		{
			case AV_CODEC_ID_H264:
				media_codec = cmn::MediaCodecId::H264;
				break;
			case AV_CODEC_ID_MP3:
				media_codec = cmn::MediaCodecId::Mp3;
				break;
			case AV_CODEC_ID_OPUS:
				media_codec = cmn::MediaCodecId::Opus;
				break;
			case AV_CODEC_ID_AAC:
				media_codec = cmn::MediaCodecId::Aac;
				break;
			default:
				media_codec = cmn::MediaCodecId::None;
				break;
		}

		if (media_type == cmn::MediaType::Unknown || media_codec == cmn::MediaCodecId::None)
		{
			logtp("Unknown media type or codec_id. media_type(%d), media_codec(%d)", media_type, media_codec);
			return nullptr;
		}

		new_track->SetId(stream->index);
		new_track->SetCodecId(media_codec);
		new_track->SetMediaType(media_type);
		new_track->SetTimeBase(stream->time_base.num, stream->time_base.den);
		new_track->SetBitrate(stream->codecpar->bit_rate);
		new_track->SetStartFrameTime(0);
		new_track->SetLastFrameTime(0);

		// Video Specific parameters
		if (media_type == cmn::MediaType::Video)
		{
			new_track->SetFrameRate(av_q2d(stream->r_frame_rate));
			new_track->SetWidth(stream->codecpar->width);
			new_track->SetHeight(stream->codecpar->height);
		}
		// Audio Specific parameters
		else if (media_type == cmn::MediaType::Audio)
		{
			new_track->SetSampleRate(stream->codecpar->sample_rate);

			cmn::AudioSample::Format sample_fmt;
			switch (stream->codecpar->format)
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
					break;
			}

			cmn::AudioChannel::Layout channel_layout;
			switch (stream->codecpar->channels)
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

			new_track->GetSample().SetFormat(sample_fmt);
			new_track->GetChannel().SetLayout(channel_layout);
		}

		return new_track;
	}

}  // namespace pvd
