//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_stream.h"

#include "base/ovlibrary/random.h"
#include "modules/rtp_rtcp/rtcp_info/sender_report.h"
#include "webrtc_application.h"
#include "webrtc_private.h"

namespace pvd
{
	std::shared_ptr<WebRTCStream> WebRTCStream::Create(StreamSourceType source_type, ov::String stream_name, uint32_t stream_id,
													   const std::shared_ptr<PushProvider> &provider,
													   const std::shared_ptr<const SessionDescription> &offer_sdp,
													   const std::shared_ptr<const SessionDescription> &peer_sdp,
													   const std::shared_ptr<Certificate> &certificate,
													   const std::shared_ptr<IcePort> &ice_port)
	{
		auto stream = std::make_shared<WebRTCStream>(source_type, stream_name, stream_id, provider, offer_sdp, peer_sdp, certificate, ice_port);
		if (stream != nullptr)
		{
			if (stream->Start() == false)
			{
				return nullptr;
			}
		}
		return stream;
	}

	WebRTCStream::WebRTCStream(StreamSourceType source_type, ov::String stream_name, uint32_t stream_id,
							   const std::shared_ptr<PushProvider> &provider,
							   const std::shared_ptr<const SessionDescription> &offer_sdp,
							   const std::shared_ptr<const SessionDescription> &peer_sdp,
							   const std::shared_ptr<Certificate> &certificate,
							   const std::shared_ptr<IcePort> &ice_port)
		: PushStream(source_type, stream_name, stream_id, provider), Node(NodeType::Edge)
	{
		_offer_sdp = offer_sdp;
		_peer_sdp = peer_sdp;
		_ice_port = ice_port;
		_certificate = certificate;
	}

	WebRTCStream::~WebRTCStream()
	{
	}

	bool WebRTCStream::Start()
	{
		std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

		logtd("[WebRTC Provider] OfferSDP");
		logtd("%s\n", _offer_sdp->ToString().CStr());
		logtd("[WebRTC Provider] AnswerSDP");
		logtd("%s", _peer_sdp->ToString().CStr());

		auto offer_media_desc_list = _offer_sdp->GetMediaList();
		auto peer_media_desc_list = _peer_sdp->GetMediaList();

		if (offer_media_desc_list.size() != peer_media_desc_list.size())
		{
			logte("m= line of answer does not correspond with offer");
			return false;
		}

		// Create Nodes
		_rtp_rtcp = std::make_shared<RtpRtcp>(RtpRtcpInterface::GetSharedPtr());
		_srtp_transport = std::make_shared<SrtpTransport>();
		_dtls_transport = std::make_shared<DtlsTransport>();

		auto application = std::static_pointer_cast<WebRTCApplication>(GetApplication());
		_dtls_transport->SetLocalCertificate(_certificate);
		_dtls_transport->StartDTLS();

		// RFC3264
		// For each "m=" line in the offer, there MUST be a corresponding "m=" line in the answer.
		std::vector<uint32_t> ssrc_list;
		for (size_t i = 0; i < peer_media_desc_list.size(); i++)
		{
			auto peer_media_desc = peer_media_desc_list[i];
			auto offer_media_desc = offer_media_desc_list[i];

			if(peer_media_desc->GetDirection() != MediaDescription::Direction::SendOnly &&
				peer_media_desc->GetDirection() != MediaDescription::Direction::SendRecv)
			{
				logtd("Media (%s) is inactive", peer_media_desc->GetMediaTypeStr().CStr());
				continue;
			}

			// The first payload has the highest priority.
			auto first_payload = peer_media_desc->GetFirstPayload();
			if (first_payload == nullptr)
			{
				logte("%s - Failed to get the first Payload type of peer sdp", GetName().CStr());
				return false;
			}

			if (peer_media_desc->GetMediaType() == MediaDescription::MediaType::Audio)
			{
				auto ssrc = peer_media_desc->GetSsrc();
				ssrc_list.push_back(ssrc);

				// Add Track
				auto audio_track = std::make_shared<MediaTrack>();

				// 	a=rtpmap:102 OPUS/48000/2
				auto codec = first_payload->GetCodec();
				auto samplerate = first_payload->GetCodecRate();
				auto channels = std::atoi(first_payload->GetCodecParams());
				RtpDepacketizingManager::SupportedDepacketizerType depacketizer_type;

				if (codec == PayloadAttr::SupportCodec::OPUS)
				{
					audio_track->SetCodecId(cmn::MediaCodecId::Opus);
					audio_track->SetOriginBitstream(cmn::BitstreamFormat::OPUS_RTP_RFC_7587);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::OPUS;
				}
				else if (codec == PayloadAttr::SupportCodec::MPEG4_GENERIC)
				{
					audio_track->SetCodecId(cmn::MediaCodecId::Aac);
					audio_track->SetOriginBitstream(cmn::BitstreamFormat::AAC_MPEG4_GENERIC);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::MPEG4_GENERIC_AUDIO;
				}
				else
				{
					logte("%s - Unsupported audio codec : %s", GetName().CStr(), first_payload->GetCodecStr().CStr());
					return false;
				}

				audio_track->SetId(ssrc);
				audio_track->SetMediaType(cmn::MediaType::Audio);
				audio_track->SetTimeBase(1, samplerate);
				audio_track->SetAudioTimestampScale(1.0);

				if (channels == 1)
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutMono);
				}
				else
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutStereo);
				}

				if (AddDepacketizer(ssrc, depacketizer_type) == false)
				{
					return false;
				}

				AddTrack(audio_track);
				_rtp_rtcp->AddRtpReceiver(ssrc, audio_track);
				_lip_sync_clock.RegisterClock(ssrc, audio_track->GetTimeBase().GetExpr());
			}
			else
			{
				auto ssrc = peer_media_desc->GetSsrc();
				ssrc_list.push_back(ssrc);

				// a=rtpmap:100 H264/90000
				auto codec = first_payload->GetCodec();
				auto timebase = first_payload->GetCodecRate();
				RtpDepacketizingManager::SupportedDepacketizerType depacketizer_type;

				auto video_track = std::make_shared<MediaTrack>();

				video_track->SetId(ssrc);
				video_track->SetMediaType(cmn::MediaType::Video);

				if (codec == PayloadAttr::SupportCodec::H264)
				{
					video_track->SetCodecId(cmn::MediaCodecId::H264);
					video_track->SetOriginBitstream(cmn::BitstreamFormat::H264_RTP_RFC_6184);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::H264;
				}
				else if (codec == PayloadAttr::SupportCodec::VP8)
				{
					video_track->SetCodecId(cmn::MediaCodecId::Vp8);
					video_track->SetOriginBitstream(cmn::BitstreamFormat::VP8_RTP_RFC_7741);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::VP8;
				}
				else
				{
					logte("%s - Unsupported video codec  : %s", GetName().CStr(), first_payload->GetCodecParams().CStr());
					return false;
				}

				video_track->SetTimeBase(1, timebase);
				video_track->SetVideoTimestampScale(1.0);

				if (AddDepacketizer(ssrc, depacketizer_type) == false)
				{
					return false;
				}

				AddTrack(video_track);
				_rtp_rtcp->AddRtpReceiver(ssrc, video_track);
				_lip_sync_clock.RegisterClock(ssrc, video_track->GetTimeBase().GetExpr());
			}
		}

		// Connect nodes
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

		_fir_timer.Start();

		return pvd::Stream::Start();
	}

	bool WebRTCStream::AddDepacketizer(uint8_t payload_type, RtpDepacketizingManager::SupportedDepacketizerType codec_id)
	{
		// Depacketizer
		auto depacketizer = RtpDepacketizingManager::Create(codec_id);
		if (depacketizer == nullptr)
		{
			logte("%s - Could not create depacketizer : codec_id(%d)", GetName().CStr(), static_cast<uint8_t>(codec_id));
			return false;
		}

		_depacketizers[payload_type] = depacketizer;

		return true;
	}

	std::shared_ptr<RtpDepacketizingManager> WebRTCStream::GetDepacketizer(uint8_t payload_type)
	{
		auto it = _depacketizers.find(payload_type);
		if (it == _depacketizers.end())
		{
			return nullptr;
		}

		return it->second;
	}

	bool WebRTCStream::Stop()
	{
		std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

		if (_rtp_rtcp != nullptr)
		{
			_rtp_rtcp->Stop();
		}

		if (_dtls_transport != nullptr)
		{
			_dtls_transport->Stop();
		}

		if (_srtp_transport != nullptr)
		{
			_srtp_transport->Stop();
		}

		ov::Node::Stop();

		return pvd::Stream::Stop();
	}

	std::shared_ptr<const SessionDescription> WebRTCStream::GetOfferSDP()
	{
		return _offer_sdp;
	}

	std::shared_ptr<const SessionDescription> WebRTCStream::GetPeerSDP()
	{
		return _peer_sdp;
	}

	bool WebRTCStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		logtd("OnDataReceived (%d)", data->GetLength());
		// To DTLS -> SRTP -> RTP|RTCP -> WebRTCStream::OnRtxpReceived

		//It must not be called during start and stop.
		std::shared_lock<std::shared_mutex> lock(_start_stop_lock);

		return SendDataToPrevNode(data);
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets)
	{
		auto first_rtp_packet = rtp_packets.front();
		auto ssrc = first_rtp_packet->Ssrc();
		logtd("%s", first_rtp_packet->Dump().CStr());

		auto track = GetTrack(ssrc);
		if (track == nullptr)
		{
			logte("%s - Could not find track : ssrc(%u)", GetName().CStr(), ssrc);
			return;
		}

		auto depacketizer = GetDepacketizer(ssrc);
		if (depacketizer == nullptr)
		{
			logte("%s - Could not find depacketizer : ssrc(%u", GetName().CStr(), ssrc);
			return;
		}

		std::vector<std::shared_ptr<ov::Data>> payload_list;
		for (const auto &packet : rtp_packets)
		{
			auto payload = std::make_shared<ov::Data>(packet->Payload(), packet->PayloadSize());
			payload_list.push_back(payload);
		}

		auto bitstream = depacketizer->ParseAndAssembleFrame(payload_list);
		if (bitstream == nullptr)
		{
			logte("%s - Could not depacketize packet : ssrc(%u)", GetName().CStr(), ssrc);
			return;
		}

		cmn::BitstreamFormat bitstream_format;
		cmn::PacketType packet_type;

		switch (track->GetCodecId())
		{
			case cmn::MediaCodecId::H264:
				// Our H264 depacketizer always converts packet to Annex B
				bitstream_format = cmn::BitstreamFormat::H264_ANNEXB;
				packet_type = cmn::PacketType::NALU;
				break;

			case cmn::MediaCodecId::Opus:
				bitstream_format = cmn::BitstreamFormat::OPUS;
				packet_type = cmn::PacketType::RAW;
				break;

			case cmn::MediaCodecId::Vp8:
				bitstream_format = cmn::BitstreamFormat::VP8;
				packet_type = cmn::PacketType::RAW;
				break;

			// It can't be reached here because it has already failed in GetDepacketizer.
			default:
				return;
		}

		auto pts = _lip_sync_clock.CalcPTS(first_rtp_packet->Ssrc(), first_rtp_packet->Timestamp());
		if(pts.has_value() == false)
		{
			logtd("not yet received sr packet : %u", first_rtp_packet->Ssrc());
			return;
		}

		auto timestamp = pts.value();
		logtd("Payload Type(%d) Timestamp(%u) PTS(%u) Time scale(%f) Adjust Timestamp(%f)",
			  first_rtp_packet->PayloadType(), first_rtp_packet->Timestamp(), timestamp, track->GetTimeBase().GetExpr(), static_cast<double>(timestamp) * track->GetTimeBase().GetExpr());

		auto frame = std::make_shared<MediaPacket>(GetMsid(),
												   track->GetMediaType(),
												   track->GetId(),
												   bitstream,
												   timestamp,
												   timestamp,
												   bitstream_format,
												   packet_type);

		logtd("Send Frame : track_id(%d) codec_id(%d) bitstream_format(%d) packet_type(%d) data_length(%d) pts(%u)", track->GetId(), track->GetCodecId(), bitstream_format, packet_type, bitstream->GetLength(), first_rtp_packet->Timestamp());

		SendFrame(frame);

		// Send FIR to reduce keyframe interval
		if (_fir_timer.IsElapsed(2000) && track->GetMediaType() == cmn::MediaType::Video)
		{
			_fir_timer.Update();
			_rtp_rtcp->SendPLI(first_rtp_packet->Ssrc());
			//_rtp_rtcp->SendFIR(_video_ssrc);
		}

		// Send Receiver Report
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info)
	{
		// Receive Sender Report
		if (rtcp_info->GetPacketType() == RtcpPacketType::SR)
		{
			auto sr = std::dynamic_pointer_cast<SenderReport>(rtcp_info);
			_lip_sync_clock.UpdateSenderReportTime(sr->GetSenderSsrc(), sr->GetMsw(), sr->GetLsw(), sr->GetTimestamp());
		}
	}

	// ov::Node Interface
	// RtpRtcp -> SRTP -> DTLS -> Edge(this)
	bool WebRTCStream::OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data)
	{
		if (ov::Node::GetNodeState() != ov::Node::NodeState::Started)
		{
			logtd("Node has not started, so the received data has been canceled.");
			return false;
		}

		return _ice_port->Send(GetId(), data);
	}

	// WebRTCStream Node has not a lower node so it will not be called
	bool WebRTCStream::OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
	{
		return true;
	}
}  // namespace pvd