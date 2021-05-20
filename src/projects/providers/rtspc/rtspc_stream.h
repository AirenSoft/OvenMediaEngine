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

#include <modules/rtp_rtcp/rtp_depacketizing_manager.h>
#include <modules/rtp_rtcp/rtp_rtcp.h>

#define RTSP_USER_AGENT_NAME	"OvenMediaEngine"
namespace pvd
{
	class RtspcProvider;

	class RtspcStream : public pvd::PullStream, public RtpRtcpInterface, public ov::Node
	{
	public:
		static std::shared_ptr<RtspcStream> Create(const std::shared_ptr<pvd::PullApplication> &application, const uint32_t stream_id, const ov::String &stream_name,	const std::vector<ov::String> &url_list);

		RtspcStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list);
		~RtspcStream() final;

		// PullStream Implementation
		int GetFileDescriptorForDetectingEvent() override;
		// If this stream belongs to the Pull provider, 
		// this function is called periodically by the StreamMotor of application. 
		// Media data has to be processed here.
		PullStream::ProcessMediaResult ProcessMediaPacket() override;

		// RtpRtcpInterface Implementation
		void OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets) override;
		void OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info) override;

		// ov::Node Interface
		bool OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data) override;
		bool OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data) override;

	private:
		std::shared_ptr<pvd::RtspcProvider> GetRtspcProvider();

		class ResponseSubscription
		{
		public:
			ResponseSubscription(const std::shared_ptr<RtspMessage> &message)
			{
				_request = message;
				_reqeust_stop_watch.Start();
			}

			std::shared_ptr<RtspMessage> WaitForResponse(uint64_t timeout_ms)
			{
				// Semaphore Wait
				if(_event.WaitFor(timeout_ms) == false)
				{
					return nullptr;
				}

				return _response;
			}

			void OnResponseReceived(const std::shared_ptr<RtspMessage> &message)
			{
				_response = message;
				_round_trip_time_ms = _reqeust_stop_watch.Elapsed();

				// Notify
				_event.Notify();
			}

		private:
			ov::StopWatch _reqeust_stop_watch;
			ov::Semaphore _event;

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

		bool SendSequenceHeaderIfNeeded();

		bool SendRequestMessage(const std::shared_ptr<RtspMessage> &message);
		std::shared_ptr<RtspMessage> ReceiveResponse(uint32_t cseq, uint64_t timeout_ms);

		// Blocking, it is used before playing state
		std::shared_ptr<RtspMessage> ReceiveMessage(int64_t timeout_msec);

		// Receive and append packet to demuxer
		bool ReceivePacket(bool non_block = false, int64_t timeout_msec = 0);

		bool AddDepacketizer(uint8_t payload_type, RtpDepacketizingManager::SupportedDepacketizerType codec_id);
		std::shared_ptr<RtpDepacketizingManager> GetDepacketizer(uint8_t payload_type);

		uint64_t AdjustTimestamp(uint8_t payload_type, uint32_t timestamp);
		uint64_t GetTimestampDelta(uint8_t payload_type, uint32_t timestamp);

		ov::String GenerateControlUrl(ov::String control);

		bool SubscribeResponse(const std::shared_ptr<RtspMessage> &request_message);
		std::shared_ptr<ResponseSubscription> PopResponseSubscription(uint32_t cseq);

		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url> _curr_url;

		std::shared_ptr<ov::Socket> _signalling_socket;
		
		// Values from RTSP
		int32_t	_cseq = 0;

		ov::String _content_base;
		ov::String _rtsp_session_id;

		uint8_t	_video_payload_type = 0;
		uint8_t _video_rtp_channel_id = 0;
		uint8_t _video_rtcp_channel_id = 0;
		ov::String _video_control;
		ov::String _video_control_url;
		std::shared_ptr<ov::Data> _h264_extradata_nalu = nullptr;

		uint8_t	_audio_payload_type = 0;
		uint8_t _audio_rtp_channel_id = 0;
		uint8_t _audio_rtcp_channel_id = 0;
		ov::String _audio_control;
		ov::String _audio_control_url;

		// CSeq : RequestMessage
		std::mutex _response_subscriptions_lock;
		std::unordered_map<uint32_t, std::shared_ptr<ResponseSubscription>> _response_subscriptions;
		RtspDemuxer _rtsp_demuxer;

		// Rtp
		std::shared_ptr<RtpRtcp>            _rtp_rtcp;
		// Payload type, Depacketizer
		std::map<uint8_t, std::shared_ptr<RtpDepacketizingManager>> _depacketizers;
		// Payload type : Timestamp
		std::map<uint8_t, uint32_t>			_last_timestamp_map;
		std::map<uint8_t, uint32_t>			_timestamp_map;

		// Statistics
		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;
		std::shared_ptr<mon::StreamMetrics> _stream_metrics;
	};
}