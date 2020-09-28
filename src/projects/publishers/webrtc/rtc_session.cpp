#include "base/info/stream.h"
#include "rtc_private.h"
#include "rtc_session.h"
#include "rtc_application.h"
#include "rtc_stream.h"

#include <utility>

std::shared_ptr<RtcSession> RtcSession::Create(const std::shared_ptr<pub::Application> &application,
                                               const std::shared_ptr<pub::Stream> &stream,
                                               const std::shared_ptr<const SessionDescription> &offer_sdp,
                                               const std::shared_ptr<const SessionDescription> &peer_sdp,
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
					   const std::shared_ptr<const SessionDescription> &offer_sdp,
					   const std::shared_ptr<const SessionDescription> &peer_sdp,
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

		// The first payload has the highest priority.
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

	std::vector<uint32_t> ssrc_list;
	for(auto media : _offer_sdp->GetMediaList())
    {
        ssrc_list.push_back(media->GetSsrc());
    }

	_rtp_rtcp = std::make_shared<RtpRtcp>((uint32_t)pub::SessionNodeType::Rtp, session, ssrc_list);


	_srtp_transport = std::make_shared<SrtpTransport>((uint32_t)pub::SessionNodeType::Srtp, session);

	_dtls_transport = std::make_shared<DtlsTransport>((uint32_t)pub::SessionNodeType::Dtls, session);
	std::shared_ptr<RtcApplication> application = std::static_pointer_cast<RtcApplication>(GetApplication());
	_dtls_transport->SetLocalCertificate(application->GetCertificate());
	_dtls_transport->StartDTLS();

	_dtls_ice_transport = std::make_shared<DtlsIceTransport>((uint32_t)pub::SessionNodeType::Ice, session, _ice_port);

	// Connect nodes
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

const std::shared_ptr<const SessionDescription>& RtcSession::GetOfferSDP() const
{
	return _offer_sdp;
}

const std::shared_ptr<const SessionDescription>& RtcSession::GetPeerSDP() const
{
	return _peer_sdp;
}

const std::shared_ptr<WebSocketClient>& RtcSession::GetWSClient()
{
	return _ws_client;
}

void RtcSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
								const std::shared_ptr<const ov::Data> &data)
{
	_received_bytes += data->GetLength();
	// ICE -> DTLS -> SRTP | SCTP -> RTP|RTCP
	_dtls_ice_transport->OnDataReceived(pub::SessionNodeType::None, data);
}

bool RtcSession::SendOutgoingData(const std::any &packet)
{
	std::shared_ptr<RtpPacket> session_packet;

	try 
	{
        session_packet = std::any_cast<std::shared_ptr<RtpPacket>>(packet);
		if(session_packet == nullptr)
		{
			return false;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream.");
		return false;
    }

	// Check if this session wants the packet
	uint32_t rtp_payload_type = session_packet->PayloadType();
	uint32_t red_block_pt = 0;
	uint32_t origin_pt_of_fec = 0;

	if(rtp_payload_type == RED_PAYLOAD_TYPE)
	{
		red_block_pt = session_packet->Header()[session_packet->HeadersSize()-1];

		// RED includes FEC packet or Media packet.
		if(session_packet->IsUlpfec())
		{
			origin_pt_of_fec = session_packet->OriginPayloadType();
		}
	}

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

	// RTP Session must be copied and sent because data is altered due to SRTP.
	auto copy_packet = std::make_shared<RtpPacket>(*session_packet);
	return _rtp_rtcp->SendOutgoingData(copy_packet);

	return true;
}