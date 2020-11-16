//
// Created by soulk on 20. 1. 20.
//

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>

#include <base/provider/pull_provider/stream.h>
#include <base/ovlibrary/semaphore.h>
#include <base/provider/pull_provider/application.h>
#include <modules/ovt_packetizer/ovt_packet.h>
#include <modules/ovt_packetizer/ovt_depacketizer.h>
#include "modules/bitstream/aac/aac.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

//TODO(Dimiden): It needs to move to configuration
#define RTSP_PULL_TIMEOUT_MSEC	10000

namespace pvd
{
	class RtspcStream : public pvd::PullStream
	{
	public:
		static std::shared_ptr<RtspcStream> Create(const std::shared_ptr<pvd::PullApplication> &application, const uint32_t stream_id, const ov::String &stream_name,	const std::vector<ov::String> &url_list);

		RtspcStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list);
		~RtspcStream() final;


		int GetFileDescriptorForDetectingEvent() override;
		// If this stream belongs to the Pull provider, 
		// this function is called periodically by the StreamMotor of application. 
		// Media data has to be processed here.
		PullStream::ProcessMediaResult ProcessMediaPacket() override;

	private:
		bool Start() override;
		bool Play() override;
		bool Stop() override;
		bool ConnectTo();
		bool RequestDescribe();
		bool RequestPlay();
		bool RequestStop();
		void Release();

		void SendSequenceHeader();
		
		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url> _curr_url;
		ov::StopWatch _stop_watch;

		AVFormatContext *_format_context = nullptr;
		AVDictionary *_format_options = nullptr;
		
		static int InterruptCallback(void *ctx);

		int64_t *_cumulative_pts = nullptr;
		int64_t *_cumulative_dts = nullptr;

		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;

		std::shared_ptr<mon::StreamMetrics> _stream_metrics;

	private:
		AacObjectType GetAacObjectType(int32_t ff_profile);
		AacSamplingFrequencies GetSamplingFrequency(int32_t ff_samplerate);
	};
}