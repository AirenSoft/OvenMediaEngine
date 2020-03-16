#include "base/info/stream.h"
#include "rtc_private.h"
#include "rtc_session.h"
#include "rtc_application.h"
#include "rtc_stream.h"

#include <utility>

std::shared_ptr<RtcSession> RtcSession::Create(const std::shared_ptr<pub::Application> &application,
                                               const std::shared_ptr<pub::Stream> &stream,
                                               const std::shared_ptr<SessionDescription> &offer_sdp,
                                               const std::shared_ptr<SessionDescription> &peer_sdp,
                                               const std::shared_ptr<IcePort> &ice_port,
											   const std::shared_ptr<WebSocketClient> &ws_client)
{
	// Session Id of the offer sdp is unique value
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), offer_sdp->GetSessionId());
	auto session = std::make_shared<RtcSession>(session_info, application, stream, offer_sdp, peer_sdp, ice_port, ws_client);
	if(!session->Start())
	{
		return nullptr;
	}
	return session;
}

RtcSession::RtcSession(const info::Session &session_info,
					   const std::shared_ptr<pub::Application> &application,
					   const std::shared_ptr<pub::Stream> &stream,
					   const std::shared_ptr<SessionDescription> &offer_sdp,
					   const std::shared_ptr<SessionDescription> &peer_sdp,
					   const std::shared_ptr<IcePort> &ice_port,
					   const std::shared_ptr<WebSocketClient> &ws_client)
	: Session(session_info, application, stream)
{
	_offer_sdp = offer_sdp;
	_peer_sdp = peer_sdp;
	_ice_port = ice_port;
	_ws_client = ws_client;
}

RtcSession::~RtcSession()
{
	Stop();
	logtd("RtcSession(%d) has been terminated finally", GetId());
}

bool RtcSession::Start()
{
	if(GetState() != SessionState::Ready)
	{
		return false;
	}

	auto session = std::static_pointer_cast<pub::Session>(GetSharedPtr());

	// Player가 준 SDP를 기준으로(플레이어가 받고자 하는 Track) RTP_RTCP를 생성한다.
	auto offer_media_desc_list = _offer_sdp->GetMediaList();
	auto peer_media_desc_list = _peer_sdp->GetMediaList();

	if(offer_media_desc_list.size() != peer_media_desc_list.size())
	{
		logte("m= line of answer does not correspond with offer");
		return false;
	}

	// RFC3264
	// For each "m=" line in the offer, there MUST be a corresponding "m=" line in the answer.
	for(size_t i = 0; i < peer_media_desc_list.size(); i++)
	{
		auto peer_media_desc = peer_media_desc_list[i];
		auto offer_media_desc = offer_media_desc_list[i];

		// 첫번째 Payload가 가장 우선순위가 높다. Server에서 제시한 Payload중 첫번째 이므로 이것으로 통신하면 됨
		auto first_payload = peer_media_desc->GetFirstPayload();
		if(first_payload == nullptr)
		{
			logte("Failed to get the first Payload type of peer sdp");
			return false;
		}

		if(peer_media_desc->GetMediaType() == MediaDescription::MediaType::Audio)
		{
			_audio_payload_type = first_payload->GetId();
		}
		else
		{
			// If there is a RED
			if(peer_media_desc->GetPayload(RED_PAYLOAD_TYPE))
			{
				_video_payload_type = RED_PAYLOAD_TYPE;
				_red_block_pt = first_payload->GetId();
			}
			else
			{
				_video_payload_type = first_payload->GetId();
			}
		}
	}

	// SessionNode를 생성하고 연결한다.
	std::vector<uint32_t> ssrc_list;
	for(auto media : _offer_sdp->GetMediaList())
    {
        ssrc_list.push_back(media->GetSsrc());
    }

    // RTP RTCP 생성
	_rtp_rtcp = std::make_shared<RtpRtcp>((uint32_t)pub::SessionNodeType::Rtp, session, ssrc_list);

	// SRTP 생성
	_srtp_transport = std::make_shared<SrtpTransport>((uint32_t)pub::SessionNodeType::Srtp, session);

	// DTLS 생성
	_dtls_transport = std::make_shared<DtlsTransport>((uint32_t)pub::SessionNodeType::Dtls, session);
	std::shared_ptr<RtcApplication> application = std::static_pointer_cast<RtcApplication>(GetApplication());
	_dtls_transport->SetLocalCertificate(application->GetCertificate());
	_dtls_transport->StartDTLS();

	// ICE-DTLS 생성
	_dtls_ice_transport = std::make_shared<DtlsIceTransport>((uint32_t)pub::SessionNodeType::Ice, session, _ice_port);

	// 노드를 연결한다.
	_rtp_rtcp->RegisterUpperNode(nullptr);
	_rtp_rtcp->RegisterLowerNode(_srtp_transport);
	_rtp_rtcp->Start();
	_srtp_transport->RegisterUpperNode(_rtp_rtcp);
	_srtp_transport->RegisterLowerNode(_dtls_transport);
	_srtp_transport->Start();
	_dtls_transport->RegisterUpperNode(_srtp_transport);
	_dtls_transport->RegisterLowerNode(_dtls_ice_transport);
	_dtls_transport->Start();
	_dtls_ice_transport->RegisterUpperNode(_dtls_transport);
	_dtls_ice_transport->RegisterLowerNode(nullptr);
	_dtls_ice_transport->Start();

	return Session::Start();
}

bool RtcSession::Stop()
{
	logtd("Stop session. Peer sdp session id : %u", GetOfferSDP()->GetSessionId());

	if(GetState() != SessionState::Started)
	{
		return true;
	}

	// 연결된 세션을 정리한다.
	if(_rtp_rtcp != nullptr)
	{
		_rtp_rtcp->Stop();
	}

	if(_dtls_ice_transport != nullptr)
	{
		_dtls_ice_transport->Stop();
	}

	if(_dtls_transport != nullptr)
	{
		_dtls_transport->Stop();
	}

	if(_srtp_transport != nullptr)
	{
		_srtp_transport->Stop();
	}

	return Session::Stop();
}

const std::shared_ptr<SessionDescription>& RtcSession::GetOfferSDP()
{
	return _offer_sdp;
}

const std::shared_ptr<SessionDescription>& RtcSession::GetPeerSDP()
{
	return _peer_sdp;
}

const std::shared_ptr<WebSocketClient>& RtcSession::GetWSClient()
{
	return _ws_client;
}

// Application에서 바로 Session의 다음 함수를 호출해준다.
void RtcSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
								const std::shared_ptr<const ov::Data> &data)
{
	_received_bytes += data->GetLength();
	// NETWORK에서 받은 Packet은 DTLS로 넘긴다.
	// ICE -> DTLS -> SRTP | SCTP -> RTP|RTCP
	_dtls_ice_transport->OnDataReceived(pub::SessionNodeType::None, data);
}

bool RtcSession::SendOutgoingData(uint32_t packet_type, const std::shared_ptr<ov::Data> &packet)
{
	auto rtp_payload_type = static_cast<uint8_t>(packet_type & 0xFF);
	auto red_block_pt = static_cast<uint8_t>((packet_type & 0xFF00) >> 8);
	auto origin_pt_of_fec = static_cast<uint8_t>((packet_type & 0xFF0000) >> 16);

	if(rtp_payload_type != _video_payload_type && rtp_payload_type != _audio_payload_type)
	{
		return false;
	}

	if(rtp_payload_type == RED_PAYLOAD_TYPE)
	{
		// When red_block_pt is ULPFEC_PAYLOAD_TYPE, origin_pt_of_fec is origin media payload type.
		if(red_block_pt != _red_block_pt && origin_pt_of_fec != _red_block_pt)
		{
			return false;
		}
	}

	_sent_bytes += packet->GetLength();

	return _rtp_rtcp->SendOutgoingData(packet);
}