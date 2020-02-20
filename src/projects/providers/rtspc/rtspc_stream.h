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

namespace pvd
{
	class RtspcStream : public pvd::Stream
	{
	public:
		static std::shared_ptr<RtspcStream>
		Create(const std::shared_ptr<pvd::Application> &application, const ov::String &stream_name,	const std::vector<ov::String> &url_list);

		RtspcStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list);

		~RtspcStream() final;

		bool IsStopThread();

	private:
		bool Start() override;
		bool Stop() override;
		void WorkerThread();
		bool ConnectTo();
		bool RequestDescribe();
		bool RequestPlay();
		bool RequestStop();

		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url>				_curr_url;
		bool _stop_thread_flag;
		std::thread _worker_thread;

		AVFormatContext *_format_context = NULL;
		static int InterruptCallback(void *ctx);

		std::shared_ptr<mon::StreamMetrics> _stream_metrics;
	};
}