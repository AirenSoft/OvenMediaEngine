#pragma once

#include <http_server/interceptors/web_socket/web_socket_client.h>
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
 * RtcSession은 RtpRtcp를 이용하여 VideoFrame/AudioSample을 Packetize를 하고
 * IcePort를 통해 전송하는 역할을 수행한다.
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
 * Application당 하나의 Thread를 돌리고, 외부 데이터 입력을 받은 Queue를 처리한다.
 * ICE/STUN 역할을 하는 IcePort는 하나만 존재한다.
 *
 * DtlsTransport는 다음과 같은 Layer에서 동작한다.
 * SRTP에서의 역할은 Key, Salt를 협상하여 전달하는 역할만 수행하고, SCTP에서는 암호화/복호화까지 담당한다.
 *
 */

class RtcApplication;
class RtcStream;

class RtcSession : public pub::Session
{
public:
	static std::shared_ptr<RtcSession> Create(const std::shared_ptr<pub::Application> &application,
	                                          const std::shared_ptr<pub::Stream> &stream,
	                                          const std::shared_ptr<SessionDescription> &offer_sdp,
	                                          const std::shared_ptr<SessionDescription> &peer_sdp,
	                                          const std::shared_ptr<IcePort> &ice_port,
											  const std::shared_ptr<WebSocketClient> &ws_client);

	RtcSession(const info::Session &session_info,
			const std::shared_ptr<pub::Application> &application,
	        const std::shared_ptr<pub::Stream> &stream,
	        const std::shared_ptr<SessionDescription> &offer_sdp,
	        const std::shared_ptr<SessionDescription> &peer_sdp,
	        const std::shared_ptr<IcePort> &ice_port,
			const std::shared_ptr<WebSocketClient> &ws_client);
	~RtcSession() override;

	bool Start() override;
	bool Stop() override;

	const std::shared_ptr<SessionDescription>& GetPeerSDP();
	const std::shared_ptr<SessionDescription>& GetOfferSDP();
	const std::shared_ptr<WebSocketClient>& GetWSClient();

	bool SendOutgoingData(uint32_t packet_type, const std::shared_ptr<ov::Data> &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info, const std::shared_ptr<const ov::Data> &data) override;

private:
	std::shared_ptr<RtpRtcp>            _rtp_rtcp;
	std::shared_ptr<SrtpTransport>      _srtp_transport;
	std::shared_ptr<DtlsTransport>      _dtls_transport;
	std::shared_ptr<DtlsIceTransport>   _dtls_ice_transport;

	std::shared_ptr<SessionDescription> _offer_sdp;
	std::shared_ptr<SessionDescription> _peer_sdp;
	std::shared_ptr<IcePort>            _ice_port;
	std::shared_ptr<WebSocketClient> 	_ws_client; // Signalling  

	uint8_t 							_red_block_pt = 0;
	uint8_t                             _video_payload_type = 0;
	uint8_t                             _audio_payload_type = 0;
};