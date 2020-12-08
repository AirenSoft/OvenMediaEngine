//
// Created by soulk on 20. 1. 20.
//


#include "base/info/application.h"
#include "rtspc_stream.h"
#include "modules/bitstream/aac/aac.h"

#define OV_LOG_TAG "RtspcStream"

namespace pvd
{
	std::shared_ptr<RtspcStream> RtspcStream::Create(const std::shared_ptr<pvd::PullApplication> &application, 
		const uint32_t stream_id, const ov::String &stream_name,
		const std::vector<ov::String> &url_list)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::RtspPull);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<RtspcStream>(application, stream_info, url_list);
		if (!stream->Start())
		{
			// Explicit deletion
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	RtspcStream::RtspcStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list)
	: pvd::PullStream(application, stream_info)
	{
		_state = State::IDLE;
		_format_context = 0;

		for(auto &url : url_list)
		{
			auto parsed_url = ov::Url::Parse(url);
			if(parsed_url)
			{
				_url_list.push_back(parsed_url);
			}
		}

		if(!_url_list.empty())
		{
			_curr_url = _url_list[0];
			SetMediaSource(_curr_url->ToUrlString(true));
		}
	}

	RtspcStream::~RtspcStream()
	{
		Stop();
		Release();
	}

	void RtspcStream::Release()
	{
		if(_format_context != nullptr)
		{
			avformat_close_input(&_format_context);
			_format_context = nullptr;
		}

		if(_format_options != nullptr)
		{
			av_dict_free(&_format_options);
			_format_options = nullptr;
		}	

		if(_cumulative_pts != nullptr)
		{
			delete[] _cumulative_pts;
		}
		
		if(_cumulative_dts != nullptr)
		{
			delete[] _cumulative_dts;
		}
	}

	bool RtspcStream::Start()
	{
		if(_state != State::IDLE)
		{
			return false;
		}

		_stop_watch.Start();

		auto begin = std::chrono::steady_clock::now();
		if (!ConnectTo())
		{
			return false;
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - begin;
		_origin_request_time_msec = static_cast<int64_t>(elapsed.count());

		begin = std::chrono::steady_clock::now();
		if (!RequestDescribe())
		{
			return false;
		}

		end = std::chrono::steady_clock::now();
		elapsed = end - begin;
		_origin_response_time_msec = static_cast<int64_t>(elapsed.count());

		return pvd::PullStream::Start();
	}

	bool RtspcStream::Play()
	{
		if (!RequestPlay())
		{
			return false;
		}

		// Stream was created completly 
		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(GetSharedPtr()));
		if(_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginRequestTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginResponseTimeMSec(_origin_response_time_msec);
		}

		return pvd::PullStream::Play();
	}

	bool RtspcStream::Stop()
	{
		// Already stopping
		if(_state != State::PLAYING)
		{
			return true;
		}
		
		if(!RequestStop())
		{
			// Force terminate 
			_state = State::ERROR;
		}

		_state = State::STOPPED;
	
		return pvd::PullStream::Stop();
	}

	bool RtspcStream::ConnectTo()
	{
		if(_state != State::IDLE && _state != State::ERROR)
		{
			return false;
		}

		logti("Requested url[%d] : %s", strlen(_curr_url->Source().CStr()), _curr_url->Source().CStr() );

		// release allocated memory
		Release();

		_format_context = avformat_alloc_context(); 
		if(_format_context == nullptr)
		{
			_state = State::ERROR;
			logte("Canot create context");
			
			return false;
		}

		// Interrupt Callback for session termination.
		_format_context->interrupt_callback.callback = InterruptCallback;
		_format_context->interrupt_callback.opaque = this;

		::av_dict_set(&_format_options, "rtsp_transport", "tcp", 0);
		
		_format_context->flags |= AVFMT_FLAG_GENPTS;
		// This is a feature added to the existing ffmpeg for OME.
		_format_context->flags |= AVFMT_FLAG_NONBLOCK;

		int err = 0;
		_stop_watch.Update();
		if ( (err = ::avformat_open_input(&_format_context, _curr_url->Source().CStr(), NULL, &_format_options))  < 0) 
		{
			_state = State::ERROR;
			
			char errbuf[256];
			av_strerror(err, errbuf, sizeof(errbuf));

			if(_stop_watch.IsElapsed(RTSP_PULL_TIMEOUT_MSEC))
			{
				logte("Failed to connect to RTSP server.(%s/%s) : Timed out", GetApplicationInfo().GetName().CStr(), GetName().CStr(), errbuf);
			}
			else
			{
				logte("Failed to connect to RTSP server.(%s/%s) : %s", GetApplicationInfo().GetName().CStr(), GetName().CStr(), errbuf);
			}

			return false;
		}

		_state = State::CONNECTED;

		return true;
	}

	bool RtspcStream::RequestDescribe()
	{
		if(_state != State::CONNECTED)
		{
			return false;
		}

		_stop_watch.Update();
		if (::avformat_find_stream_info(_format_context, NULL) < 0) 
		{
			_state = State::ERROR;        	
			logte("Could not find stream information");

			return false;
		}

		for (uint32_t track_id = 0; track_id < _format_context->nb_streams; track_id++)
		{
			AVStream *stream = _format_context->streams[track_id];

			logtd("[%d] media_type[%d] codec_id[%d], extradata_size[%d] tb[%d/%d]", track_id, stream->codecpar->codec_type, stream->codecpar->codec_id, stream->codecpar->extradata_size, stream->time_base.num, stream->time_base.den);

			auto new_track = std::make_shared<MediaTrack>();

			cmn::MediaType media_type = 
				(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)?cmn::MediaType::Video:
				(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)?cmn::MediaType::Audio:
				cmn::MediaType::Unknown;

			cmn::MediaCodecId media_codec = 
				(stream->codecpar->codec_id == AV_CODEC_ID_H264)?cmn::MediaCodecId::H264:
				(stream->codecpar->codec_id == AV_CODEC_ID_H265)?cmn::MediaCodecId::H265:
				(stream->codecpar->codec_id == AV_CODEC_ID_VP8)?cmn::MediaCodecId::Vp8:
				(stream->codecpar->codec_id == AV_CODEC_ID_VP9)?cmn::MediaCodecId::Vp9:
				(stream->codecpar->codec_id == AV_CODEC_ID_AAC)?cmn::MediaCodecId::Aac:
				(stream->codecpar->codec_id == AV_CODEC_ID_MP3)?cmn::MediaCodecId::Mp3:
				(stream->codecpar->codec_id == AV_CODEC_ID_OPUS)?cmn::MediaCodecId::Opus:
				cmn::MediaCodecId::None;

			if (media_type == cmn::MediaType::Unknown || media_codec == cmn::MediaCodecId::None)
			{
				logtp("Unknown media type or codec_id. media_type(%d), media_codec(%d)", media_type, media_codec);

				continue;
			}

			new_track->SetId(track_id);
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
			else if(media_type == cmn::MediaType::Audio)
			{
				new_track->SetSampleRate(stream->codecpar->sample_rate);
				new_track->GetSample().SetFormat(static_cast<cmn::AudioSample::Format>(cmn::AudioSample::Format::S16P));
				new_track->GetChannel().SetLayout(static_cast<cmn::AudioChannel::Layout>(
					(stream->codecpar->channels==1)?cmn::AudioChannel::Layout::LayoutMono:
					(stream->codecpar->channels==2)?cmn::AudioChannel::Layout::LayoutStereo:
													cmn::AudioChannel::Layout::LayoutUnknown));
			}

			AddTrack(new_track);
		}

		// Sometimes the values of PTS/DTS are negative or incorrect(invalid pts) 
		// Decided to calculate PTS/DTS as the cumulative value of the packet duration.
		_cumulative_pts = new int64_t[_format_context->nb_streams];
		_cumulative_dts = new int64_t[_format_context->nb_streams];

		for(uint32_t track_id=0 ; track_id<_format_context->nb_streams ; ++track_id)
		{
			_cumulative_pts[track_id] = 0;
			_cumulative_dts[track_id] = 0;
		}

		_state = State::DESCRIBED;

		return true;
	}

	bool RtspcStream::RequestPlay()
	{
		if(_state != State::DESCRIBED)
		{
			return false;
		}

		SendSequenceHeader();

		_state = State::PLAYING;

		return true;
	}

	bool RtspcStream::RequestStop()
	{
		if (_state != State::PLAYING)
		{
			return false;
		}

		_state = State::STOPPING;

		return true;
	}

	int RtspcStream::GetFileDescriptorForDetectingEvent()
	{
		// This is a feature added to the existing ffmpeg for OME.
		return av_get_iformat_file_descriptor(_format_context);
	}

	PullStream::ProcessMediaResult RtspcStream::ProcessMediaPacket()
	{
		AVPacket packet;
		av_init_packet(&packet);
		packet.size = 0;
		packet.data = nullptr;

		bool is_eof = false;
		bool is_received_first_packet = false;

		_stop_watch.Update();
		int32_t ret = ::av_read_frame(_format_context, &packet);
		if ( ret < 0 )
		{
			// This is a feature added to the existing ffmpeg for OME.
			if (ret == AVERROR(EAGAIN))
			{
				return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
			}

			if(_stop_watch.IsElapsed(RTSP_PULL_TIMEOUT_MSEC))
			{
				return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
			}

			if ( (ret == AVERROR_EOF || ::avio_feof(_format_context->pb)) && !is_eof)
			{
				// If EOF is not receiving packets anymore, end thread.
				logti("%s/%s(%u) RtspcStream has finished.", GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
				_state = State::STOPPED;
				is_eof = true;
				return ProcessMediaResult::PROCESS_MEDIA_FINISH;
			}

			if (_format_context->pb && _format_context->pb->error)
			{
				// If the connection is broken, terminate the thread.
				logte("%s/%s(%u) RtspcStream's connection has broken.", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), GetId());
				_state = State::ERROR;
				return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
			}
		}
		else
		{
			is_eof = false;
		}

		// If the first packet is received as NOPTS_VALUE, reset the PTS value to zero.
		if (!is_received_first_packet)
		{
			if (packet.pts == AV_NOPTS_VALUE)
			{
				packet.pts = 0;
			}
			if (packet.dts == AV_NOPTS_VALUE)
			{	
				packet.dts = 0;
			}

			is_received_first_packet = true;
		}

		auto track = GetTrack(packet.stream_index);
		if(track == nullptr)
		{
			::av_packet_unref(&packet);
			return ProcessMediaResult::PROCESS_MEDIA_TRY_AGAIN;
		}

		// Accumulate PTS/DTS
		// TODO : Require Verification
		if(packet.duration != 0)
		{
			_cumulative_pts[packet.stream_index] += packet.duration;
			_cumulative_dts[packet.stream_index] += packet.duration;
		}
		else
		{
			_cumulative_pts[packet.stream_index] = packet.pts;
			_cumulative_dts[packet.stream_index] = packet.dts;
		}

		auto media_type = track->GetMediaType();
		auto codec_id = track->GetCodecId();
		cmn::BitstreamFormat bitstream_format = cmn::BitstreamFormat::Unknown;
		cmn::PacketType packet_type = cmn::PacketType::Unknown;

		if(codec_id == cmn::MediaCodecId::H264)
		{
			bitstream_format = cmn::BitstreamFormat::H264_ANNEXB;
			packet_type = cmn::PacketType::NALU;
		}
		else if(codec_id == cmn::MediaCodecId::H265)
		{
			bitstream_format = cmn::BitstreamFormat::H265_ANNEXB;
			packet_type = cmn::PacketType::NALU;
		}
		else if(codec_id == cmn::MediaCodecId::Aac)
		{
			if(AACAdts::IsValid(packet.data, packet.size) == true)
				bitstream_format = cmn::BitstreamFormat::AAC_ADTS;
			else
				bitstream_format = cmn::BitstreamFormat::AAC_LATM;
	
			packet_type = cmn::PacketType::RAW;
		}

		// Make MediaPacket from AVPacket
		auto data = std::make_shared<ov::Data>(packet.data, packet.size);
		auto media_packet = std::make_shared<MediaPacket>(media_type, 
														track->GetId(), 
														data,
														_cumulative_pts[packet.stream_index], 
														_cumulative_dts[packet.stream_index], 
														bitstream_format, 
														packet_type);

		SendFrame(media_packet);

		::av_packet_unref(&packet);

		return ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}


	void RtspcStream::SendSequenceHeader()
	{
		for(uint32_t track_id=0 ; track_id<_format_context->nb_streams ; ++track_id)
		{
			AVStream *stream = _format_context->streams[track_id];

			auto track = GetTrack(stream->index);
			if(track == nullptr)
			{
				continue;
			}

			auto media_type = track->GetMediaType();
			auto codec_id = track->GetCodecId();

			if(codec_id == cmn::MediaCodecId::H264)
			{
				// Send SPS/PPS Nalunit
				auto media_packet = std::make_shared<MediaPacket>(media_type, 
					track->GetId(), 
					std::make_shared<ov::Data>(stream->codecpar->extradata, stream->codecpar->extradata_size),
					0, 
					0, 
					cmn::BitstreamFormat::H264_ANNEXB, 
					cmn::PacketType::NALU);

				SendFrame(media_packet);
			}
			else if(codec_id == cmn::MediaCodecId::H265)
			{
				// Nothing to do here
			}			
			else if(codec_id == cmn::MediaCodecId::Aac)
			{
				AACSpecificConfig aac_config;
				aac_config.SetOjbectType(GetAacObjectType(stream->codecpar->profile));
				aac_config.SetSamplingFrequency(GetSamplingFrequency(stream->codecpar->sample_rate));
				aac_config.SetChannel(stream->codecpar->channels);

				std::vector<uint8_t> sequence_header;
				aac_config.Serialize(sequence_header);
				
				auto media_packet = std::make_shared<MediaPacket>(media_type, 
					track->GetId(), 
					std::make_shared<ov::Data>(&sequence_header[0], sequence_header.size()),
					0, 
					0, 
					cmn::BitstreamFormat::AAC_LATM, 
					cmn::PacketType::SEQUENCE_HEADER);

				SendFrame(media_packet);
			}
		}
	}

	AacObjectType RtspcStream::GetAacObjectType(int32_t ff_profile)
	{
		AacObjectType aac_profile = AacObjectTypeAacMain;
		switch(ff_profile)
		{
			case FF_PROFILE_AAC_MAIN:
				aac_profile = AacObjectTypeAacMain;
				break;
			case FF_PROFILE_AAC_LOW:
				aac_profile = AacObjectTypeAacLC;
				break;
			case FF_PROFILE_AAC_SSR:
				aac_profile = AacObjectTypeAacSSR;
				break;
			case FF_PROFILE_AAC_LTP:
				aac_profile = AacObjecttypeAacLTP;
				break;
			default:
				break;
		}

		return aac_profile;
	}

	AacSamplingFrequencies RtspcStream::GetSamplingFrequency(int32_t ff_samplerate)
	{
		AacSamplingFrequencies aac_sample_rate = 
			(ff_samplerate == 96000)?RATES_96000HZ:
			(ff_samplerate == 88200)?RATES_88200HZ:
			(ff_samplerate == 64000)?RATES_64000HZ:
			(ff_samplerate == 48000)?RATES_48000HZ:
			(ff_samplerate == 44100)?RATES_44100HZ:
			(ff_samplerate == 32000)?RATES_32000HZ:
			(ff_samplerate == 24000)?RATES_24000HZ:
			(ff_samplerate == 22050)?RATES_22050HZ:
			(ff_samplerate == 16000)?RATES_16000HZ:
			(ff_samplerate == 12000)?RATES_12000HZ:
			(ff_samplerate == 11025)?RATES_11025HZ:
			(ff_samplerate == 7350)?RATES_8000HZ:EXPLICIT_RATE;

		return aac_sample_rate;
	}

	int RtspcStream::InterruptCallback(void *ctx)
	{
		if(ctx != nullptr)
		{
			auto obj = (RtspcStream*)ctx;
			if(obj->_stop_watch.IsElapsed(RTSP_PULL_TIMEOUT_MSEC))
			{
				// timed out
				return true;
			}
		}

		return false;
	}
}



