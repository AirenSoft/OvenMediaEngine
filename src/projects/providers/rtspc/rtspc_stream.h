//
// Created by soulk on 20. 1. 20.
//

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>

#include <base/provider/stream.h>
#include <base/ovlibrary/semaphore.h>
#include <base/provider/application.h>
#include <modules/ovt_packetizer/ovt_packet.h>
#include <modules/ovt_packetizer/ovt_depacketizer.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

//TODO(Dimiden): It needs to move to configuration
#define RTSP_PULL_TIMEOUT_MSEC	3000

namespace pvd
{
	class RtspcStream : public pvd::Stream
	{
	public:
		static std::shared_ptr<RtspcStream> Create(const std::shared_ptr<pvd::Application> &application, const uint32_t stream_id, const ov::String &stream_name,	const std::vector<ov::String> &url_list);

		RtspcStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list);
		~RtspcStream() final;

	private:
		bool Start() override;
		bool Play() override;
		bool Stop() override;
		bool IsStopThread();
		void WorkerThread();
		bool ConnectTo();
		bool RequestDescribe();
		bool RequestPlay();
		bool RequestStop();
		void Release();

		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url>				_curr_url;
		bool _stop_thread_flag;
		std::thread _worker_thread;
		ov::StopWatch _stop_watch;

		AVFormatContext *_format_context = nullptr;
		AVDictionary *_format_options = nullptr;
		
		static int InterruptCallback(void *ctx);

		std::shared_ptr<mon::StreamMetrics> _stream_metrics;
	};
}