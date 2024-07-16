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
	std::shared_ptr<WebRTCStream> WebRTCStream::Create(StreamSourceType source_type, ov::String stream_name,
													   const std::shared_ptr<PushProvider> &provider,
													   const std::shared_ptr<const SessionDescription> &local_sdp,
													   const std::shared_ptr<const SessionDescription> &remote_sdp,
													   const std::shared_ptr<Certificate> &certificate,
													   const std::shared_ptr<IcePort> &ice_port,
													   session_id_t ice_session_id)
	{
		auto stream = std::make_shared<WebRTCStream>(source_type, stream_name, provider, local_sdp, remote_sdp, certificate, ice_port, ice_session_id);
		if (stream != nullptr)
		{
			if (stream->Start() == false)
			{
				return nullptr;
			}
		}
		return stream;
	}

	WebRTCStream::WebRTCStream(StreamSourceType source_type, ov::String stream_name,
							   const std::shared_ptr<PushProvider> &provider,
							   const std::shared_ptr<const SessionDescription> &local_sdp,
							   const std::shared_ptr<const SessionDescription> &remote_sdp,
							   const std::shared_ptr<Certificate> &certificate,
							   const std::shared_ptr<IcePort> &ice_port,
							   session_id_t ice_session_id)
		: PushStream(source_type, stream_name, provider), Node(NodeType::Edge)
	{
		_local_sdp = local_sdp;
		_remote_sdp = remote_sdp;

		if (_local_sdp->GetType() == SessionDescription::SdpType::Offer)
		{
			_offer_sdp = _local_sdp;
			_answer_sdp = _remote_sdp;
		}
		else
		{
			_offer_sdp = _remote_sdp;
			_answer_sdp = _local_sdp;
		}

		_ice_port = ice_port;
		_certificate = certificate;
		_session_key = ov::Random::GenerateString(8);
		_ice_session_id = ice_session_id;

		_h264_bitstream_parser.SetConfig(H264BitstreamParser::Config{._parse_slice_type = true});
	}

	WebRTCStream::~WebRTCStream()
	{
		logtd("WebRTCStream(%s/%d) is destroyed", GetName().CStr(), GetId());
	}

	bool WebRTCStream::Start()
	{
		std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

		logtd("[WebRTC Provider] Local SDP");
		logtd("%s\n", _local_sdp->ToString().CStr());
		logtd("[WebRTC Provider] Peer SDP");
		logtd("%s", _remote_sdp->ToString().CStr());

		auto local_media_desc_list = _local_sdp->GetMediaList();
		auto remote_media_desc_list = _remote_sdp->GetMediaList();
		auto answer_media_desc_list = _answer_sdp->GetMediaList();

		if (local_media_desc_list.size() != remote_media_desc_list.size())
		{
			logte("m= line of peer does not correspond with local");
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
		// For each "m=" line in the local, there MUST be a corresponding "m=" line in the peer.
		std::vector<uint32_t> ssrc_list;
		for (size_t i = 0; i < remote_media_desc_list.size(); i++)
		{
			auto remote_media_desc = remote_media_desc_list[i];
			auto local_media_desc = local_media_desc_list[i];
			auto answer_media_desc = answer_media_desc_list[i];

			if(remote_media_desc->GetDirection() != MediaDescription::Direction::SendOnly &&
				remote_media_desc->GetDirection() != MediaDescription::Direction::SendRecv)
			{
				logtd("Media (%s) is inactive", remote_media_desc->GetMediaTypeStr().CStr());
				continue;
			}

			// The first payload has the highest priority.
			auto first_payload = answer_media_desc->GetFirstPayload();
			if (first_payload == nullptr)
			{
				logte("%s - Failed to get the first Payload type of peer sdp", GetName().CStr());
				return false;
			}

			if (answer_media_desc->GetMediaType() == MediaDescription::MediaType::Audio)
			{
				auto ssrc = remote_media_desc->GetSsrc();
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

				if (_rtp_rtcp->IsTransportCcFeedbackEnabled() == false && first_payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::TransportCc) == true)
				{
					// a=extmap:id http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
					uint8_t transport_cc_extension_id = 0;
					ov::String transport_cc_extension_uri;
					if (answer_media_desc->FindExtmapItem("transport-wide-cc-extensions", transport_cc_extension_id, transport_cc_extension_uri) == true)
					{
						_rtp_rtcp->EnableTransportCcFeedback(transport_cc_extension_id);
					}
				}

				RegisterRtpClock(ssrc, audio_track->GetTimeBase().GetExpr());
			}
			else
			{
				auto ssrc = remote_media_desc->GetSsrc();
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
					_h264_extradata_nalu = first_payload->GetH264ExtraDataAsAnnexB();
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

				if (_rtp_rtcp->IsTransportCcFeedbackEnabled() == false && first_payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::TransportCc) == true)
				{
					// a=extmap:id http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
					uint8_t transport_cc_extension_id = 0;
					ov::String transport_cc_extension_uri;
					if (answer_media_desc->FindExtmapItem("transport-wide-cc-extensions", transport_cc_extension_id, transport_cc_extension_uri) == true)
					{
						_rtp_rtcp->EnableTransportCcFeedback(transport_cc_extension_id);
					}
				}

				// uri:ietf:rtc:rtp-hdrext:video:CompositionTime
				ov::String cts_extmap_uri;
				if (answer_media_desc->FindExtmapItem("CompositionTime", _cts_extmap_id, cts_extmap_uri))
				{
					_cts_extmap_enabled = true;
				}
				
				RegisterRtpClock(ssrc, video_track->GetTimeBase().GetExpr());
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

		_sent_sequence_header = false;

		return pvd::Stream::Start();
	}

	ov::String WebRTCStream::GetSessionKey() const
	{
		return _session_key;
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

		if (GetState() == Stream::State::STOPPED || GetState() == Stream::State::TERMINATED)
		{
			return true;
		}

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

		_ice_port->DisconnectSession(_ice_session_id);

		ov::Node::Stop();

		return pvd::Stream::Stop();
	}

	std::shared_ptr<const SessionDescription> WebRTCStream::GetLocalSDP()
	{
		return _local_sdp;
	}

	std::shared_ptr<const SessionDescription> WebRTCStream::GetPeerSDP()
	{
		return _remote_sdp;
	}

	bool WebRTCStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		logtp("OnDataReceived (%d)", data->GetLength());
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
		logtp("%s", first_rtp_packet->Dump().CStr());

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
			logtp("%s", packet->Dump().CStr());
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

		bool cts_enabled = false;
		switch (track->GetCodecId())
		{
			case cmn::MediaCodecId::H264:
				// Our H264 depacketizer always converts packet to Annex B
				bitstream_format = cmn::BitstreamFormat::H264_ANNEXB;
				packet_type = cmn::PacketType::NALU;
				cts_enabled = _cts_extmap_enabled == true;
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

		int64_t adjusted_timestamp;
		if (AdjustRtpTimestamp(first_rtp_packet->Ssrc(), first_rtp_packet->Timestamp(), std::numeric_limits<uint32_t>::max(), adjusted_timestamp) == false)
		{
			logtd("not yet received sr packet : %u", first_rtp_packet->Ssrc());
			// Prevents the stream from being deleted because there is no input data
			MonitorInstance->IncreaseBytesIn(*Stream::GetSharedPtr(), bitstream->GetLength());
			return;
		}
		
		int64_t dts = adjusted_timestamp;
		if (cts_enabled == true)
		{
			auto cts_extension_opt = first_rtp_packet->GetExtension<int24_t>(_cts_extmap_id);
			if (cts_extension_opt.has_value() == true)
			{
				int32_t cts = cts_extension_opt.value();
				dts = adjusted_timestamp - (cts * 90);
				logtd("PTS(%lld) CTS(%d) DTS(%lld)", adjusted_timestamp, cts, dts);
			}
			else
			{
				logte("CTS is enabled but not found in the packet");
			}
		}
		
		logtd("Payload Type(%d) Timestamp(%u) PTS(%u) Time scale(%f) Adjust Timestamp(%f)",
			  first_rtp_packet->PayloadType(), first_rtp_packet->Timestamp(), adjusted_timestamp, track->GetTimeBase().GetExpr(), static_cast<double>(adjusted_timestamp) * track->GetTimeBase().GetExpr());

		auto frame = std::make_shared<MediaPacket>(GetMsid(),
												   track->GetMediaType(),
												   track->GetId(),
												   bitstream,
												   adjusted_timestamp,
												   dts,
												   bitstream_format,
												   packet_type);

		// Reorder frames in DTS order
		if (cts_enabled == true)
		{
			// PTS order to DTS order
			// Q and Flush (if slice type is I or P)
			_dts_ordered_frame_buffer.emplace(dts, frame);

			switch (bitstream_format)
			{
				case cmn::BitstreamFormat::H264_ANNEXB:
				{
					if (_h264_bitstream_parser.Parse(bitstream) == true)
					{
						auto last_slice_type = _h264_bitstream_parser.GetLastSliceType();

						logtd("PTS(%lld) DTS(%lld) Slice Type(%d)", adjusted_timestamp, dts, last_slice_type.has_value()?static_cast<int>(last_slice_type.value()):-1);

						if (last_slice_type.has_value() == true && last_slice_type.value() != H264SliceType::B)
						{
							// Flush All
							for (auto &frame : _dts_ordered_frame_buffer)
							{
								OnFrame(track, frame.second);
							}
							_dts_ordered_frame_buffer.clear();
						}
					}

					return;
				}
				default:
					break;
			}
		}

		OnFrame(track, frame);
	}

	void WebRTCStream::OnFrame(const std::shared_ptr<MediaTrack> &track, const std::shared_ptr<MediaPacket> &media_packet)
	{
		logtd("Send Frame : track_id(%d) codec_id(%d) bitstream_format(%d) packet_type(%d) data_length(%d) pts(%u)", track->GetId(), track->GetCodecId(), media_packet->GetBitstreamFormat(), media_packet->GetPacketType(), media_packet->GetDataLength(), media_packet->GetPts());

		// This may not work since almost WebRTC browser sends SRS/PPS in-band
		if (_sent_sequence_header == false && track->GetCodecId() == cmn::MediaCodecId::H264 && _h264_extradata_nalu != nullptr)
		{
			auto sps_pps_packet = std::make_shared<MediaPacket>(GetMsid(),	
																track->GetMediaType(), 
																track->GetId(), 
																_h264_extradata_nalu,
																media_packet->GetPts(), 
																media_packet->GetDts(), 
																cmn::BitstreamFormat::H264_ANNEXB, 
																cmn::PacketType::NALU);
			SendFrame(sps_pps_packet);
			_sent_sequence_header = true;
		}

		SendFrame(media_packet);

		// Send FIR to reduce keyframe interval
		if (_fir_timer.IsElapsed(3000) && track->GetMediaType() == cmn::MediaType::Video)
		{
			_fir_timer.Update();
			//_rtp_rtcp->SendPLI(first_rtp_packet->Ssrc());
			_rtp_rtcp->SendFIR(track->GetId());
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
			UpdateSenderReportTimestamp(sr->GetSenderSsrc(), sr->GetMsw(), sr->GetLsw(), sr->GetTimestamp());
		}
	}

	// ov::Node Interface
	// RtpRtcp -> SRTP -> DTLS -> Edge(this) -> IcePort
	bool WebRTCStream::OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data)
	{
		if (ov::Node::GetNodeState() != ov::Node::NodeState::Started)
		{
			logtd("Node has not started, so the received data has been canceled.");
			return false;
		}

		return _ice_port->Send(_ice_session_id, data);
	}

	// WebRTCStream Node has not a lower node so it will not be called
	bool WebRTCStream::OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
	{
		return true;
	}
}  // namespace pvd