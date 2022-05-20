//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>
#include <base/provider/pull_provider/application.h>
#include <base/provider/pull_provider/stream.h>
#include <modules/rtp_rtcp/lip_sync_clock.h>
#include <modules/rtp_rtcp/rtp_depacketizing_manager.h>
#include <modules/rtp_rtcp/rtp_rtcp.h>
#include <modules/rtsp/header_fields/rtsp_header_fields.h>
#include <modules/rtsp/rtsp_demuxer.h>
#include <modules/rtsp/rtsp_message.h>
#include <modules/sdp/session_description.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

namespace pvd
{
	class FileProvider;

	class FileStream : public pvd::PullStream
	{
	public:
		static std::shared_ptr<FileStream> Create(const std::shared_ptr<pvd::PullApplication> &application, const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list, std::shared_ptr<pvd::PullStreamProperties> properties);

		FileStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list, std::shared_ptr<pvd::PullStreamProperties> properties);
		~FileStream() final;

		ProcessMediaEventTrigger GetProcessMediaEventTriggerMode() override
		{
			return ProcessMediaEventTrigger::TRIGGER_INTERVAL;
		}

		// PullStream Implementation 
		int GetFileDescriptorForDetectingEvent() override
		{
			return 0;
		}

		// If this stream belongs to the Pull provider,
		// this function is called periodically by the StreamMotor of application.
		// Media data has to be processed here.
		PullStream::ProcessMediaResult ProcessMediaPacket() override;

	private:
		std::shared_ptr<pvd::FileProvider> GetFileProvider();

		bool StartStream(const std::shared_ptr<const ov::Url> &url) override;	 // Start
		bool RestartStream(const std::shared_ptr<const ov::Url> &url) override;	 // Failover
		bool StopStream() override;												 // Stop

		bool ConnectTo();
		bool RequestDescribe();
		bool RequestPlay();
		bool RequestStop();
		void Release();

		void SendSequenceHeader();

		std::shared_ptr<const ov::Url> _url;

		// Payload type : Timestamp
		std::map<uint8_t, uint32_t> _last_timestamp_map;
		std::map<uint8_t, uint32_t> _timestamp_map;

		ov::StopWatch _play_request_time;

		bool _sent_sequence_header = false;

		// Statistics
		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;
		std::shared_ptr<mon::StreamMetrics> _stream_metrics;

		static std::shared_ptr<MediaTrack> AvStreamToMediaTrack(AVStream *stream);
		static std::shared_ptr<MediaPacket> AvPacketToMediaPacket(AVPacket *src, cmn::MediaType media_type, cmn::BitstreamFormat format, cmn::PacketType packet_type);

		std::shared_ptr<AVFormatContext> CreateFormatContext();
		AVFormatContext *_format_context;
	};
}  // namespace pvd