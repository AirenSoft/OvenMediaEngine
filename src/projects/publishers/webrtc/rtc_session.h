#pragma once

#include <modules/http_server/interceptors/web_socket/web_socket_client.h>
#include "base/info/media_track.h"
#include "base/publisher/session.h"
#include "modules/sdp/session_description.h"
#include "modules/ice/ice_port.h"
#include "modules//dtls_srtp/dtls_ice_transport.h"
#include "modules/rtp_rtcp/rtp_rtcp.h"
#include "modules/rtp_rtcp/rtp_rtcp_interface.h"
#include "modules/dtls_srtp/dtls_transport.h"
#include <unordered_set>

/*
 *
 *
 *   - Send -						  - Recv -
 * [MediaRouter]					[  ICEPort  ]
 * [ Publisher ]					[ Publisher ]
 * [Application][Stream Queue]		[Application][Packet Queue]
 * [  Stream   ]					      |
 * [  Session  ]					[  Session	]
 * -------------------------------------------------------
 * 					Session Node
 * -------------------------------------------------------
 * [  RTP_RTCP ]					[  ICE/STUN ]
 * [SRTP] [SCTP]					[    DTLS	]
 * [    DTLS   ]					[SRTP] [SCTP]
 * [  ICE/STUN ]					[  RTP_RTCP	]
 *
 */

class RtcApplication;
class RtcStream;

class RtcSession : public pub::Session
{
public:
	static std::shared_ptr<RtcSession> Create(const std::shared_ptr<pub::Application> &application,
	                                          const std::shared_ptr<pub::Stream> &stream,
	                                          const std::shared_ptr<const SessionDescription> &offer_sdp,
	                                          const std::shared_ptr<const SessionDescription> &peer_sdp,
	                                          const std::shared_ptr<IcePort> &ice_port,
											  const std::shared_ptr<WebSocketClient> &ws_client);

	RtcSession(const info::Session &session_info,
			const std::shared_ptr<pub::Application> &application,
	        const std::shared_ptr<pub::Stream> &stream,
	        const std::shared_ptr<const SessionDescription> &offer_sdp,
	        const std::shared_ptr<const SessionDescription> &peer_sdp,
	        const std::shared_ptr<IcePort> &ice_port,
			const std::shared_ptr<WebSocketClient> &ws_client);
	~RtcSession() override;

	bool Start() override;
	bool Stop() override;

	const std::shared_ptr<const SessionDescription>& GetPeerSDP() const;
	const std::shared_ptr<const SessionDescription>& GetOfferSDP() const;
	const std::shared_ptr<WebSocketClient>& GetWSClient();

	bool SendOutgoingData(const std::any &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info, const std::shared_ptr<const ov::Data> &data) override;

	void OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info);

private:
	bool ProcessNACK(const std::shared_ptr<RtcpInfo> &rtcp_info);

	std::shared_ptr<RtpRtcp>            _rtp_rtcp;
	std::shared_ptr<SrtpTransport>      _srtp_transport;
	std::shared_ptr<DtlsTransport>      _dtls_transport;
	std::shared_ptr<DtlsIceTransport>   _dtls_ice_transport;

	std::shared_ptr<const SessionDescription> _offer_sdp;
	std::shared_ptr<const SessionDescription> _peer_sdp;
	std::shared_ptr<IcePort>            _ice_port;
	std::shared_ptr<WebSocketClient> 	_ws_client; // Signalling  

	uint8_t 							_red_block_pt = 0;
	uint8_t                             _video_payload_type = 0;
	uint32_t							_video_ssrc = 0;
	uint32_t							_video_rtx_ssrc = 0;
	uint8_t                             _audio_payload_type = 0;
	uint32_t							_audio_ssrc = 0;

	bool								_use_rtx_flag = false;


	uint16_t							_rtx_sequence_number = 1;
};