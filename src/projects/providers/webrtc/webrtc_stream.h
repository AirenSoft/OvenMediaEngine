//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/provider/push_provider/stream.h"
#include "modules/ice/ice_port.h"
#include "modules/sdp/session_description.h"

#include "modules/rtp_rtcp/rtp_rtcp.h"
#include "modules/rtp_rtcp/rtp_packetizer_interface.h"
#include "modules/dtls_srtp/dtls_transport.h"
#include "modules/dtls_srtp/dtls_ice_transport.h"
#include "modules/rtp_rtcp/rtp_depacketizing_manager.h"

namespace pvd
{
	class WebRTCStream : public pvd::PushStream, public RtpRtcpInterface
	{
	public:
		static std::shared_ptr<WebRTCStream> Create(StreamSourceType source_type, ov::String stream_name, uint32_t stream_id, 
													const std::shared_ptr<PushProvider> &provider,
													const std::shared_ptr<const SessionDescription> &offer_sdp,
													const std::shared_ptr<const SessionDescription> &peer_sdp,
													const std::shared_ptr<Certificate> &certificate, 
													const std::shared_ptr<IcePort> &ice_port);
		
		explicit WebRTCStream(StreamSourceType source_type, ov::String stream_name, uint32_t stream_id, 
								const std::shared_ptr<PushProvider> &provider,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp,
								const std::shared_ptr<Certificate> &certificate, 
								const std::shared_ptr<IcePort> &ice_port);
		~WebRTCStream() final;

		bool Start() override;
		bool Stop() override;

		std::shared_ptr<const SessionDescription> GetOfferSDP();
		std::shared_ptr<const SessionDescription> GetPeerSDP();

		// ------------------------------------------
		// Implementation of PushStream
		// ------------------------------------------
		PushStreamType GetPushStreamType() override
		{
			return PushStream::PushStreamType::DATA;
		}
		bool OnDataReceived(const std::shared_ptr<const ov::Data> &data) override;

		// RtpRtcpInterface Implement
		void OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets) override;
		void OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info) override;

	private:
		bool AddDepacketizer(uint8_t payload_type, cmn::MediaCodecId codec_id);
		std::shared_ptr<RtpDepacketizingManager> GetDepacketizer(uint8_t payload_type);

		bool SendFIR();

		uint8_t _fir_seq = 0;
		ov::StopWatch _fir_timer;

		std::shared_ptr<const SessionDescription> _offer_sdp;
		std::shared_ptr<const SessionDescription> _peer_sdp;
		std::shared_ptr<IcePort> _ice_port;
		std::shared_ptr<Certificate> _certificate;

		std::shared_ptr<RtpRtcp>            _rtp_rtcp;
		std::shared_ptr<SrtpTransport>      _srtp_transport;
		std::shared_ptr<DtlsTransport>      _dtls_transport;
		std::shared_ptr<DtlsIceTransport>   _dtls_ice_transport;

		uint8_t 							_red_block_pt = 0;
		uint8_t                             _video_payload_type = 0;
		uint32_t							_video_ssrc = 0;
		uint32_t							_video_rtx_ssrc = 0;
		uint8_t                             _audio_payload_type = 0;
		uint32_t							_audio_ssrc = 0;
		bool								_rtx_enabled = false;
		uint16_t							_rtx_sequence_number = 1;
		uint64_t							_session_expired_time = 0;

		std::shared_mutex					_start_stop_lock;

		// Payload type, Depacketizer
		std::map<uint8_t, std::shared_ptr<RtpDepacketizingManager>> _depacketizers;
	};
}