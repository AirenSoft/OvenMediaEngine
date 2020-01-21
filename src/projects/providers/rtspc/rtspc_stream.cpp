//
// Created by soulk on 20. 1. 20.
//


#include "media_router/bitstream/avc_video_packet_fragmentizer.h"
#include "rtspc_stream.h"

#define OV_LOG_TAG "RtspcStream"

namespace pvd
{
	std::shared_ptr<RtspcStream> RtspcStream::Create(const std::shared_ptr<pvd::Application> &application, const ov::String &stream_name,
					  						const std::vector<ov::String> &url_list)
	{
		StreamInfo stream_info(StreamSourceType::RTSPC_PROVIDER);

		stream_info.SetId(application->IssueUniqueStreamId());
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

	RtspcStream::RtspcStream(const std::shared_ptr<pvd::Application> &application, const StreamInfo &stream_info, const std::vector<ov::String> &url_list)
			: pvd::Stream(application, stream_info)
	{
		av_register_all();
		avformat_network_init();

		_stop_thread_flag = false;
		_state = State::IDLE;
		_format_context = 0;

		for(auto &url : url_list)
		{
			auto parsed_url = ov::Url::Parse(url.CStr());
			if(parsed_url)
			{
				_url_list.push_back(parsed_url);
			}
		}

		if(!_url_list.empty())
		{
			_curr_url = _url_list[0];
		}


#if 1
		// -------------------------------------------------------------------------------
		// TODO(soulk): It is a temporary code. Import the RTSP URL from the bash script.
		// -------------------------------------------------------------------------------
		// Below is sample code for GET_URL.SH.
		//	
		// 		#!/bin/bash
		// 		RTSP_URL=$(wget -qO- http://211.235.108.156:50202/hls/live1)
		// 		echo $RTSP_URL

		FILE* stream = popen( "/home/soulk/rtsp/get_url.sh", "r" );

		std::ostringstream output;

		while ( !feof(stream) && !ferror(stream) )
		{
			char buf[128];
			int32_t bytesRead = fread( buf, 1, 128, stream );
			output.write( buf, bytesRead );
		}

		ov::String result = output.str().c_str();
		result = result.Replace("\n", "");

		_curr_url = ov::Url::Parse(result.CStr());
#endif
	}

	RtspcStream::~RtspcStream()
	{
		Stop();
	}

	bool RtspcStream::Start()
	{
		if (_stop_thread_flag)
		{
			return false;
		}

		if (!ConnectTo())
		{
			return false;
		}

		if (!RequestDescribe())
		{
			return false;
		}

		if (!RequestPlay())
		{
			return false;
		}

		_stop_thread_flag = false;
		_worker_thread = std::thread(&RtspcStream::WorkerThread, this);
		_worker_thread.detach();

		return true;
	}

	bool RtspcStream::Stop()
	{
		logtd("Stop");

		if(_state == State::STOPPED || _state == State::IDLE)
		{
			return false;
		}

		// TODO(soulk): If the thread is in lock state at av_read_frame, let's use AVIOInterruptCB to get out of lock.
		_stop_thread_flag = true;

		if(_state == State::PLAYING || _state == State::CONNECTED || _state == State::DESCRIBED)
		{
			RequestStop();
		}

    	if (_format_context)
    	{
        	avformat_close_input(&_format_context);
        	_format_context = nullptr;
    	}

		// It will be deleted later when the Provider tries to create a stream which is same name.
		// Because it cannot delete it self.
		_state = State::STOPPED;

		return true;
	}

	bool RtspcStream::ConnectTo()
	{
		if(_state != State::IDLE && _state != State::ERROR)
		{
			return false;
		}

		// logtd("Requested url : %s", _curr_url->Source().CStr());

		AVDictionary *options = NULL;
		::av_dict_set(&options, "rtsp_transport", "tcp", 0);

    	if (::avformat_open_input(&_format_context, _curr_url->Source().CStr(), NULL, &options)  < 0) {
        	_state = State::ERROR;
        	logte("Cannot open input file");
        	
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

		if (::avformat_find_stream_info(_format_context, NULL) < 0) {
        	_state = State::ERROR;        	
        	logte("Could not find stream information");

        	return false;
        }

		for (uint32_t track_id = 0; track_id < _format_context->nb_streams; track_id++)
		{
			AVStream *stream = _format_context->streams[track_id];

			logtp("[%d] media_type[%d] codec_id[%d]", track_id, stream->codecpar->codec_type, stream->codecpar->codec_id);

			auto new_track = std::make_shared<MediaTrack>();

			common::MediaType media_type = 	(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)?common::MediaType::Video:
											(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)?common::MediaType::Audio:
											common::MediaType::Unknown;

			common::MediaCodecId media_codec = (stream->codecpar->codec_id == AV_CODEC_ID_H264)?common::MediaCodecId::H264:
											   (stream->codecpar->codec_id == AV_CODEC_ID_VP8)?common::MediaCodecId::Vp8:
											   (stream->codecpar->codec_id == AV_CODEC_ID_VP9)?common::MediaCodecId::Vp9:
											   (stream->codecpar->codec_id == AV_CODEC_ID_FLV1)?common::MediaCodecId::Flv:
											   (stream->codecpar->codec_id == AV_CODEC_ID_AAC)?common::MediaCodecId::Aac:
											   (stream->codecpar->codec_id == AV_CODEC_ID_MP3)?common::MediaCodecId::Mp3:
											   (stream->codecpar->codec_id == AV_CODEC_ID_OPUS)?common::MediaCodecId::Opus:
											   common::MediaCodecId::None;


			if (media_type == common::MediaType::Unknown || media_codec == common::MediaCodecId::None)
			{
				logte("Unknown media type or codec_id. media_type(%d), media_codec(%d)", media_type, media_codec);

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
			if (media_type == common::MediaType::Video)
			{
				new_track->SetFrameRate(av_q2d(stream->r_frame_rate));
				new_track->SetWidth(stream->codecpar->width);
				new_track->SetHeight(stream->codecpar->height);
			}
			// Audio Specific parameters
			else if(media_type == common::MediaType::Audio)
			{
				new_track->SetSampleRate(stream->codecpar->sample_rate);
				
				// new_track->GetSample().SetFormat(static_cast<common::AudioSample::Format>(json_audio_track["sampleFormat"].asInt()));
				// new_track->GetChannel().SetLayout(static_cast<common::AudioChannel::Layout>(json_audio_track["layout"].asUInt()));
			}

			AddTrack(new_track);
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

		_state = State::PLAYING;

		return true;
	}

	bool RtspcStream::RequestStop()
	{
		if (_state != State::PLAYING)
		{
			return false;
		}

		if (_worker_thread.joinable())
		{
			_worker_thread.join();
			logtd("Packet receive thread terminated.");
		}

		return true;
	}

	void RtspcStream::WorkerThread()
	{
		AVPacket packet;
		packet.size = 0;
		packet.data = nullptr;

		bool is_eof = false;
		bool is_received_first_packet = false;

		while (!_stop_thread_flag)
		{
			int32_t ret = ::av_read_frame(_format_context, &packet);
			if ( ret < 0 )
			{
				if ( (ret == AVERROR_EOF || ::avio_feof(_format_context->pb)) && !is_eof)
				{
					// End Of Stream
					logtd("End of file");
					is_eof = true;
				}
				if (_format_context->pb && _format_context->pb->error)
				{
					logte("Connection is broken");
					break;
				}

				// logtw("Error by timeout. Receive packets again.");
				continue;
			}
			else
			{
				is_eof = true;
			}

			// If the first packet is received as NOPTS_VALUE, reset the PTS value to zero.
			// TODO(soulk) : If the first packet per track is NOPTS_VALUE, the code below should be modify.
			if (!is_received_first_packet)
			{
				if (packet.pts == AV_NOPTS_VALUE)
					packet.pts = 0;
				if (packet.dts == AV_NOPTS_VALUE)
					packet.dts = 0;

				is_received_first_packet = true;
			}

			logtp("track_id(%d), flags(%d), pts(%10lld), dts(%10lld), size(%d)", packet.stream_index, packet.flags, packet.pts, packet.dts, packet.size);


			auto flag = (packet.flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag;
			auto media_packet = std::make_shared<MediaPacket>(common::MediaType::Video, 0, packet.data, packet.size, packet.pts, packet.dts, packet.duration, flag);

			_application->SendFrame(GetSharedPtrAs<StreamInfo>(), media_packet);

			::av_packet_unref(&packet);
		}
	}
}