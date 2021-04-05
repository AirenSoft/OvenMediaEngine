//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>

#include <base/provider/pull_provider/stream.h>
#include <base/provider/pull_provider/application.h>

#include <modules/rtsp/rtsp_message.h>
#include <modules/rtsp/rtsp_demuxer.h>

#define RTSP_USER_AGENT_NAME	"OvenMediaEngine"
namespace pvd
{
	class RtspcProvider;

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
		std::shared_ptr<pvd::RtspcProvider> GetRtspcProvider();

		class RequestResponse
		{
		public:
			RequestResponse(const std::shared_ptr<RtspMessage> &message)
			{
				_request = message;
				_reqeust_stop_watch.Start();
			}

			std::shared_ptr<RtspMessage> WaitForResponse(uint64_t timeout_ms)
			{
				// Semaphore Wait
				
				return _response;
			}

			void OnResponseReceived(const std::shared_ptr<RtspMessage> &message)
			{
				_response = message;
				_round_trip_time_ms = _reqeust_stop_watch.Elapsed();

				// Notify
			}

		private:
			ov::StopWatch _reqeust_stop_watch;
			int64_t _round_trip_time_ms = 0;

			std::shared_ptr<RtspMessage> _request = nullptr;
			std::shared_ptr<RtspMessage> _response = nullptr;
		};

		bool Start() override;
		bool Play() override;
		bool Stop() override;

		bool ConnectTo();
		bool RequestDescribe();
		bool RequestSetup();
		bool RequestPlay();
		bool RequestStop();
		void Release();

		int32_t GetNextCSeq();
		int32_t	_cseq = 0;

		bool SendRequestMessage(const std::shared_ptr<RtspMessage> &message);
		std::shared_ptr<RtspMessage> WaitForResponse(const std::shared_ptr<RequestResponse>& request, uint64_t timeout_ms);

		// Blocking, it is used before playing state
		std::shared_ptr<RtspMessage> ReceiveMessage(int64_t timeout_msec);

		// Receive and append packet to demuxer
		bool ReceivePacket(bool non_block = false, int64_t timeout_msec = 0);

		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url> _curr_url;

		std::shared_ptr<ov::Socket> _signalling_socket;

		uint8_t	_video_payload_type = 0;
		uint8_t	_audio_payload_type = 0;

		

		// CSeq, RequestMessage
		std::unordered_map<uint32_t, std::shared_ptr<RequestResponse>> _requested_map;
		RtspDemuxer _rtsp_demuxer;


		// Statistics
		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;

		std::shared_ptr<mon::StreamMetrics> _stream_metrics;
	};
}