//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "base/info/stream.h"
#include "rtc_private.h"
#include "rtc_session.h"
#include "webrtc_publisher.h"
#include "rtc_application.h"
#include "rtc_stream.h"
#include "rtc_common_types.h"

#include "modules/rtp_rtcp/rtcp_info/nack.h"

#include <utility>

std::shared_ptr<RtcSession> RtcSession::Create(const std::shared_ptr<WebRtcPublisher> &publisher,
											   const std::shared_ptr<pub::Application> &application,
                                               const std::shared_ptr<pub::Stream> &stream,
											   const ov::String &file_name,
                                               const std::shared_ptr<const SessionDescription> &offer_sdp,
                                               const std::shared_ptr<const SessionDescription> &peer_sdp,
                                               const std::shared_ptr<IcePort> &ice_port,
											   const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session)
{
	// Session Id of the offer sdp is unique value
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), offer_sdp->GetSessionId());
	auto session = std::make_shared<RtcSession>(session_info, publisher, application, stream, file_name, offer_sdp, peer_sdp, ice_port, ws_session);
	if(!session->Start())
	{
		return nullptr;
	}
	return session;
}

RtcSession::RtcSession(const info::Session &session_info,
					   const std::shared_ptr<WebRtcPublisher> &publisher,
					   const std::shared_ptr<pub::Application> &application,
					   const std::shared_ptr<pub::Stream> &stream,
					   const ov::String &file_name,
					   const std::shared_ptr<const SessionDescription> &offer_sdp,
					   const std::shared_ptr<const SessionDescription> &peer_sdp,
					   const std::shared_ptr<IcePort> &ice_port,
					   const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session)
	: Session(session_info, application, stream), Node(NodeType::Edge)
{
	_publisher = publisher;
	_offer_sdp = offer_sdp;
	_peer_sdp = peer_sdp;
	_ice_port = ice_port;
	_ws_session = ws_session;
	_file_name = file_name;
}

RtcSession::~RtcSession()
{
	Stop();
	logtd("RtcSession(%d) has been terminated finally", GetId());
}

bool RtcSession::Start()
{
	// start and stop must be called independently.
	std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

	if(pub::Session::GetState() != SessionState::Ready)
	{
		return false;
	}

	logtd("[WebRTC Publisher] OfferSDP");
	logtd("%s\n", _offer_sdp->ToString().CStr());
	logtd("[WebRTC Publisher] AnswerSDP");
	logtd("%s", _peer_sdp->ToString().CStr());

	auto offer_media_desc_list = _offer_sdp->GetMediaList();
	auto peer_media_desc_list = _peer_sdp->GetMediaList();

	if(offer_media_desc_list.size() != peer_media_desc_list.size())
	{
		logte("m= line of answer does not correspond with offer");
		return false;
	}

	// Create nodes

	_rtp_rtcp = std::make_shared<RtpRtcp>(RtpRtcpInterface::GetSharedPtr());
	_srtp_transport = std::make_shared<SrtpTransport>();

	_dtls_transport = std::make_shared<DtlsTransport>();
	std::shared_ptr<RtcApplication> application = std::static_pointer_cast<RtcApplication>(GetApplication());
	_dtls_transport->SetLocalCertificate(application->GetCertificate());
	_dtls_transport->StartDTLS();

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
			_audio_ssrc = offer_media_desc->GetSsrc();
			_rtp_rtcp->AddRtpSender(_audio_payload_type, _audio_ssrc, first_payload->GetCodecRate(), offer_media_desc->GetCname());
		}
		else
		{
			_video_payload_type = first_payload->GetId();
			_video_ssrc = offer_media_desc->GetSsrc();
			_rtp_rtcp->AddRtpSender(_video_payload_type, _video_ssrc, first_payload->GetCodecRate(), offer_media_desc->GetCname());

			auto payload = peer_media_desc->GetPayload(static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE));
			if(payload != nullptr)
			{
				if(payload->GetCodec() == PayloadAttr::SupportCodec::RED)
				{	
					_red_enabled = true;
				}
			}

			// Retransmission, We always define the RTX payload as payload + 1
			payload = peer_media_desc->GetPayload(_video_payload_type+1);
			if(payload != nullptr)
			{
				if(payload->GetCodec() == PayloadAttr::SupportCodec::RTX)
				{
					_rtx_enabled = true;
				}
			}
		}
	}

	// Get Playlist
	_playlist = std::static_pointer_cast<RtcStream>(GetStream())->GetRtcPlaylist(_file_name, 
																				CodecIdFromPayloadTypeNumber(_video_payload_type), 
																				CodecIdFromPayloadTypeNumber(_audio_payload_type));
	if (_playlist == nullptr)
	{
		logte("Failed to get the playlist (%s/%s/%s) because there is no available rendition", GetApplication()->GetName().CStr(), GetStream()->GetName().CStr(), _file_name.CStr());
		return false;
	}

	_current_rendition = _playlist->GetFirstRendition();

	auto current_video_track = _current_rendition->GetVideoTrack();
	auto current_audio_track = _current_rendition->GetAudioTrack();

	logtc("Video PT(%d) Audio PT(%d) Video TrackID(%u) Audio TrackID(%u)", _video_payload_type, _audio_payload_type, 
														current_video_track ? current_video_track->GetId() : -1,
														current_audio_track ? current_audio_track->GetId() : -1);

	// Connect nodes

	// [RTP_RTCP] -> [SRTP] -> [DTLS] -> [RtcSession(Edge)]

	_rtp_rtcp->RegisterPrevNode(nullptr);
	_rtp_rtcp->RegisterNextNode(_srtp_transport);
	_rtp_rtcp->Start();
	_srtp_transport->RegisterPrevNode(_rtp_rtcp);
	_srtp_transport->RegisterNextNode(_dtls_transport);
	_srtp_transport->Start();
	_dtls_transport->RegisterPrevNode(_srtp_transport);
	_dtls_transport->RegisterNextNode(ov::Node::GetSharedPtr());
	_dtls_transport->Start();

	RegisterPrevNode(_dtls_transport);
	RegisterNextNode(nullptr);
	ov::Node::Start();

	_abr_test_watch.Start();

	return Session::Start();
}

bool RtcSession::Stop()
{
	// start and stop must be called independently.
	std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

	logtd("Stop session. Peer sdp session id : %u", GetOfferSDP()->GetSessionId());

	if(pub::Session::GetState() != SessionState::Started && pub::Session::GetState() != SessionState::Stopping)
	{
		return true;
	}

	if(_rtp_rtcp != nullptr)
	{
		_rtp_rtcp->Stop();
	}

	if(_dtls_transport != nullptr)
	{
		_dtls_transport->Stop();
	}

	if(_srtp_transport != nullptr)
	{
		_srtp_transport->Stop();
	}

	// TODO(Getroot): Doesn't need this?
	//_ws_session->Close();

	ov::Node::Stop();

	return Session::Stop();
}

void RtcSession::SetSessionExpiredTime(uint64_t expired_time)
{
	_session_expired_time = expired_time;
}

const std::shared_ptr<const SessionDescription>& RtcSession::GetOfferSDP() const
{
	return _offer_sdp;
}

const std::shared_ptr<const SessionDescription>& RtcSession::GetPeerSDP() const
{
	return _peer_sdp;
}

const std::shared_ptr<http::svr::ws::WebSocketSession>& RtcSession::GetWSClient()
{
	return _ws_session;
}

bool RtcSession::ChangeRendition(const ov::String &rendition_name)
{
	auto rendition = _playlist->GetRendition(rendition_name);
	if(rendition == nullptr)
	{
		logte("Failed to get the rendition (%s) because there is no available rendition", rendition_name.CStr());
		return false;
	}

	if (rendition == _current_rendition)
	{
		logtd("The rendition (%s) is already selected", rendition_name.CStr());
		return true;
	}

	_next_rendition = rendition;

	return true;
}

void RtcSession::OnMessageReceived(const std::any &message)
{
	//It must not be called during start and stop.
	std::shared_lock<std::shared_mutex> lock(_start_stop_lock);

	std::shared_ptr<const ov::Data> data = nullptr;
	try 
	{
        data = std::any_cast<std::shared_ptr<const ov::Data>>(message);
		if(data == nullptr)
		{
			return;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream.");
		return;
    }

	_received_bytes += data->GetLength();

	logtd("Received %u bytes", _received_bytes);

	// RTP_RTCP -> SRTP -> DTLS -> Edge Node(RtcSession)
	SendDataToPrevNode(data);
}

bool RtcSession::IsSelectedPacket(const std::shared_ptr<const RtpPacket> &rtp_packet)
{
	logtd("RTP PT(%d) TrackID(%u) => Video PT(%d) Audio PT(%d) Video TrackID(%d) Audio TrackID(%d)", rtp_packet->PayloadType(), rtp_packet->GetTrackId(),
											_video_payload_type, _audio_payload_type, 
											_current_rendition->GetVideoTrack() ? _current_rendition->GetVideoTrack()->GetId() : -1, 
											_current_rendition->GetAudioTrack() ? _current_rendition->GetAudioTrack()->GetId() : -1);

	if (_next_rendition != nullptr)
	{
		// Try to change rendition
		// When a video packet of the next rendition is a keyframe
		if (_next_rendition->GetVideoTrack() != nullptr && _next_rendition->GetVideoTrack()->GetId() == rtp_packet->GetTrackId() && rtp_packet->IsKeyframe())
		{
			_current_rendition = _next_rendition;
			_next_rendition = nullptr;
		}
		// Can change rendition immediately if next rendition has only audio track
		else if (_next_rendition->GetVideoTrack() == nullptr)
		{
			_current_rendition = _next_rendition;
			_next_rendition = nullptr;
		}
	}

	// Select Track for ABR
	auto selected_track = rtp_packet->IsVideoPacket() ? _current_rendition->GetVideoTrack() : _current_rendition->GetAudioTrack();
	if (selected_track == nullptr || rtp_packet->GetTrackId() != selected_track->GetId())
	{
		return false;
	}

	uint32_t rtp_payload_type = rtp_packet->PayloadType(); 

	// Select Payload Type
	if (_red_enabled == false && rtp_payload_type == static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE))
	{
		return false;
	}
	// If red is enabled and if the packet type is video, only RED is selected.
	else if (_red_enabled == true && rtp_packet->IsVideoPacket() && rtp_payload_type != static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE))
	{
		return false;
	}

	// Get original payload type in the RED packet
	if (rtp_payload_type == static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE))
	{
		auto red_packet = std::dynamic_pointer_cast<const RedRtpPacket>(rtp_packet);

		// RED includes FEC packet or Media packet.
		if(rtp_packet->IsUlpfec())
		{
			rtp_payload_type = red_packet->OriginPayloadType();
		}
		else
		{
			rtp_payload_type = red_packet->BlockPT();
		}
	}

	if(rtp_payload_type == _audio_payload_type || rtp_payload_type == _video_payload_type)
	{
		return true;
	}

	return false;
}

void RtcSession::SendOutgoingData(const std::any &packet)
{
	//It must not be called during start and stop.
	std::shared_lock<std::shared_mutex> lock(_start_stop_lock);

	if(pub::Session::GetState() != SessionState::Started)
	{
		return;
	}

	// Check expired time
	if(_session_expired_time != 0 && _session_expired_time < ov::Clock::NowMSec())
	{
		_publisher->DisconnectSession(pub::Session::GetSharedPtrAs<RtcSession>());
		SetState(SessionState::Stopping);
		return;
	}

	std::shared_ptr<RtpPacket> session_packet;

	try 
	{
        session_packet = std::any_cast<std::shared_ptr<RtpPacket>>(packet);
		if(session_packet == nullptr)
		{
			return;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream.");
		return;
    }

	// Check the packet is selected.
	if (IsSelectedPacket(session_packet) == false)
	{
		return;
	}

	// RTP Session must be copied and sent because data is altered due to SRTP.
	auto copy_packet = std::make_shared<RtpPacket>(*session_packet);

	if (copy_packet->IsVideoPacket())
	{
		copy_packet->SetSequenceNumber(_video_rtp_sequence_number++);
	}
	else
	{
		copy_packet->SetSequenceNumber(_audio_rtp_sequence_number++);
	}

	// rtp_rtcp -> srtp -> dtls -> Edge Node(RtcSession)
	_rtp_rtcp->SendRtpPacket(copy_packet);

	MonitorInstance->IncreaseBytesOut(*GetStream(), PublisherType::Webrtc, copy_packet->GetData()->GetLength());
}

void RtcSession::OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets)
{
	// No player send RTP packet 
}

void RtcSession::OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info)
{
	if(pub::Session::GetState() != SessionState::Started)
	{
		return;
	}

	if(rtcp_info->GetPacketType() == RtcpPacketType::RR)
	{
		// Process
	}
	else if(rtcp_info->GetPacketType() == RtcpPacketType::RTPFB)
	{
		if(rtcp_info->GetCountOrFmt() == static_cast<uint8_t>(RTPFBFMT::NACK))
		{
			// Process
			ProcessNACK(rtcp_info);
		}
	}

	rtcp_info->DebugPrint();
}


bool RtcSession::ProcessNACK(const std::shared_ptr<RtcpInfo> &rtcp_info)
{
	if(_rtx_enabled == false)
	{
		return true;
	}

	auto stream = std::dynamic_pointer_cast<RtcStream>(GetStream());
	if(stream == nullptr)
	{
		return false;
	}
	
	auto nack = std::dynamic_pointer_cast<NACK>(rtcp_info);
	if(nack->GetMediaSsrc() != _video_ssrc)
	{
		return false;
	}

	// Retransmission
	for(size_t i=0; i<nack->GetLostIdCount(); i++)
	{
		auto seq_no = nack->GetLostId(i);
		auto packet = stream->GetRtxRtpPacket(_video_payload_type, seq_no);
		if(packet != nullptr)
		{
			logd("RTCP", "Send RTX packet : %u/%u", _video_payload_type, seq_no);
			auto copy_packet = std::make_shared<RtpPacket>(*(std::dynamic_pointer_cast<RtpPacket>(packet)));
			copy_packet->SetSequenceNumber(_rtx_sequence_number++);
			return _rtp_rtcp->SendRtpPacket(copy_packet);
		}
	}

	return true;
}

// ov::Node Interface
// RtpRtcp -> SRTP -> DTLS -> Edge(this)
bool RtcSession::OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	if(ov::Node::GetNodeState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	return _ice_port->Send(GetId(), data);
}

// RtcSession Node has not a lower node so it will not be called
bool RtcSession::OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	return true;
}