//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_stream.h"
#include "webrtc_application.h"
#include "webrtc_private.h"

#include "modules/rtp_rtcp/rtcp_info/sender_report.h"

#include "base/ovlibrary/random.h"

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
		if(stream != nullptr)
		{
			if(stream->Start() == false)
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

		if(offer_media_desc_list.size() != peer_media_desc_list.size())
		{
			logte("m= line of answer does not correspond with offer");
			return false;
		}

		_local_ssrc = ov::Random::GenerateUInt32();

		// Create Nodes
		_rtp_rtcp = std::make_shared<RtpRtcp>(RtpRtcpInterface::GetSharedPtr());
		_srtp_transport = std::make_shared<SrtpTransport>();
		_dtls_transport = std::make_shared<DtlsTransport>();

		auto application = std::static_pointer_cast<WebRTCApplication>(GetApplication());
		_dtls_transport->SetLocalCertificate(_certificate);
		_dtls_transport->StartDTLS();

		double audio_lip_sync_timebase = 0, video_lip_sync_timebase = 0;

		// RFC3264
		// For each "m=" line in the offer, there MUST be a corresponding "m=" line in the answer.
		std::vector<uint32_t> ssrc_list;
		for(size_t i = 0; i < peer_media_desc_list.size(); i++)
		{
			auto peer_media_desc = peer_media_desc_list[i];
			auto offer_media_desc = offer_media_desc_list[i];

			// The first payload has the highest priority.
			auto first_payload = peer_media_desc->GetFirstPayload();
			if(first_payload == nullptr)
			{
				logte("%s - Failed to get the first Payload type of peer sdp", GetName().CStr());
				return false;
			}

			if(peer_media_desc->GetMediaType() == MediaDescription::MediaType::Audio)
			{
				_audio_payload_type = first_payload->GetId();
				_audio_ssrc = peer_media_desc->GetSsrc();
				ssrc_list.push_back(_audio_ssrc);

				// Add Track
				auto audio_track = std::make_shared<MediaTrack>();

				// 	a=rtpmap:102 OPUS/48000/2
				auto codec = first_payload->GetCodec();
				auto samplerate = first_payload->GetCodecRate();
				auto channels = std::atoi(first_payload->GetCodecParams());
				RtpDepacketizingManager::SupportedDepacketizerType depacketizer_type;

				if(codec == PayloadAttr::SupportCodec::OPUS)
				{
					audio_track->SetCodecId(cmn::MediaCodecId::Opus);
					audio_track->SetOriginBitstream(cmn::BitstreamFormat::OPUS_RTP_RFC_7587);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::OPUS;
				}
				else if(codec == PayloadAttr::SupportCodec::MPEG4_GENERIC)
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
				
				audio_track->SetId(first_payload->GetId());
				audio_track->SetMediaType(cmn::MediaType::Audio);
				audio_track->SetTimeBase(1, samplerate);
				audio_lip_sync_timebase = audio_track->GetTimeBase().GetExpr();
				audio_track->SetAudioTimestampScale(1.0);

				if (channels == 1)
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutMono);
				}
				else
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutStereo);
				}

				if(AddDepacketizer(_audio_payload_type, depacketizer_type) == false)
				{
					return false;
				}

				AddTrack(audio_track);
				_rtp_rtcp->AddRtpReceiver(_audio_payload_type, audio_track);
			}
			else
			{
				// RED is not support yet
				if(0 && peer_media_desc->GetPayload(static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE)))
				{
					_video_payload_type = static_cast<uint8_t>(FixedRtcPayloadType::RED_PAYLOAD_TYPE);
					_red_block_pt = first_payload->GetId();
				}
				else
				{
					_video_payload_type = first_payload->GetId();
				}

				// Retransmission, We always define the RTX payload as payload + 1
				auto payload = peer_media_desc->GetPayload(_video_payload_type+1);
				if(payload != nullptr)
				{
					if(payload->GetCodec() == PayloadAttr::SupportCodec::RTX)
					{
						_rtx_enabled = true;
						_video_rtx_ssrc = peer_media_desc->GetRtxSsrc();
					}
				}

				_video_ssrc = peer_media_desc->GetSsrc();
				ssrc_list.push_back(_video_ssrc);
				
				// a=rtpmap:100 H264/90000
				auto codec = first_payload->GetCodec();
				auto timebase = first_payload->GetCodecRate();
				RtpDepacketizingManager::SupportedDepacketizerType depacketizer_type;

				auto video_track = std::make_shared<MediaTrack>();

				video_track->SetId(first_payload->GetId());
				video_track->SetMediaType(cmn::MediaType::Video);

				if(codec == PayloadAttr::SupportCodec::H264)
				{
					video_track->SetCodecId(cmn::MediaCodecId::H264);
					video_track->SetOriginBitstream(cmn::BitstreamFormat::H264_RTP_RFC_6184);
					depacketizer_type = RtpDepacketizingManager::SupportedDepacketizerType::H264;
				}
				else if(codec == PayloadAttr::SupportCodec::VP8)
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
				video_lip_sync_timebase = video_track->GetTimeBase().GetExpr();
				video_track->SetVideoTimestampScale(1.0);

				if(AddDepacketizer(_video_payload_type, depacketizer_type) == false)
				{
					return false;
				}

				AddTrack(video_track);
				_rtp_rtcp->AddRtpReceiver(_video_payload_type, video_track);
			}
		}

		// lip sync clock
		_lip_sync_clock = std::make_shared<LipSyncClock>(audio_lip_sync_timebase, video_lip_sync_timebase);

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
		if(depacketizer == nullptr)
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
		if(it == _depacketizers.end())
		{
			return nullptr;
		}

		return it->second;
	}

	bool WebRTCStream::Stop()
	{
		std::lock_guard<std::shared_mutex> lock(_start_stop_lock);

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

	uint64_t WebRTCStream::AdjustTimestamp(uint8_t payload_type, uint32_t timestamp)
	{
		uint32_t curr_timestamp; 

		if(_timestamp_map.find(payload_type) == _timestamp_map.end())
		{
			curr_timestamp = 0;
		}
		else
		{
			curr_timestamp = _timestamp_map[payload_type];
		}

		curr_timestamp += GetTimestampDelta(payload_type, timestamp);

		_timestamp_map[payload_type] = curr_timestamp;

		return curr_timestamp;
	}

	uint64_t WebRTCStream::GetTimestampDelta(uint8_t payload_type, uint32_t timestamp)
	{
		// First timestamp
		if(_last_timestamp_map.find(payload_type) == _last_timestamp_map.end())
		{
			_last_timestamp_map[payload_type] = timestamp;
			// Start with zero
			return 0;
		}

		auto delta = timestamp - _last_timestamp_map[payload_type];
		_last_timestamp_map[payload_type] = timestamp;

		return delta;
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets)
	{
		auto first_rtp_packet = rtp_packets.front();
		auto payload_type = first_rtp_packet->PayloadType();
		logtd("%s", first_rtp_packet->Dump().CStr());

		auto track = GetTrack(payload_type);
		if(track == nullptr)
		{
			logte("%s - Could not find track : payload_type(%d)", GetName().CStr(), payload_type);
			return;
		}

		auto depacketizer = GetDepacketizer(payload_type);
		if(depacketizer == nullptr)
		{
			logte("%s - Could not find depacketizer : payload_type(%d)", GetName().CStr(), payload_type);
			return;
		}

		std::vector<std::shared_ptr<ov::Data>> payload_list;
		for(const auto &packet : rtp_packets)
		{
			auto payload = std::make_shared<ov::Data>(packet->Payload(), packet->PayloadSize());
			payload_list.push_back(payload);
		}
		
		auto bitstream = depacketizer->ParseAndAssembleFrame(payload_list);
		if(bitstream == nullptr)
		{
			logte("%s - Could not depacketize packet : payload_type(%d)", GetName().CStr(), payload_type);
			return;
		}

		cmn::BitstreamFormat bitstream_format;
		cmn::PacketType packet_type;

		switch(track->GetCodecId())
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

		[[maybe_unused]]ClockType clock_type = ClockType::NUMBER_OF_TYPE;
		if(track->GetMediaType() == cmn::MediaType::Audio)
		{
			clock_type = ClockType::AUDIO;
		}
		else
		{
			clock_type = ClockType::VIDEO;
		}

		//auto timestamp = _lip_sync_clock->GetNextTimestamp(clock_type, first_rtp_packet->Timestamp());
		//auto timestamp = AdjustTimestamp(first_rtp_packet->PayloadType(), first_rtp_packet->Timestamp());
		auto timestamp = first_rtp_packet->Timestamp();

		logtd("Payload Type(%d) Timestamp(%u) Timestamp Delta(%u) Time scale(%f) Adjust Timestamp(%f)", 
				first_rtp_packet->PayloadType(), first_rtp_packet->Timestamp(), timestamp, track->GetTimeBase().GetExpr(), static_cast<double>(timestamp) * track->GetTimeBase().GetExpr());

		auto frame = std::make_shared<MediaPacket>(track->GetMediaType(),
											  track->GetId(),
											  bitstream,
											  timestamp,
											  timestamp,
											  bitstream_format,
											  packet_type);

		logtd("Send Frame : track_id(%d) codec_id(%d) bitstream_format(%d) packet_type(%d) data_length(%d) pts(%u)",  track->GetId(),  track->GetCodecId(), bitstream_format, packet_type, bitstream->GetLength(), first_rtp_packet->Timestamp());
		
		SendFrame(frame);

		// Send FIR to reduce keyframe interval
		if(_fir_timer.IsElapsed(1000))
		{
			_fir_timer.Update();
			_rtp_rtcp->SendFir(_video_ssrc);
		}
		
		// Send Receiver Report
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info)
	{
		// Receive Sender Report
		if(rtcp_info->GetPacketType() == RtcpPacketType::SR)
		{
			auto sr = std::dynamic_pointer_cast<SenderReport>(rtcp_info);
			ClockType clock_type = ClockType::NUMBER_OF_TYPE;
			if(sr->GetSenderSsrc() == _video_ssrc)
			{
				clock_type = ClockType::VIDEO;
			}
			else if(sr->GetSenderSsrc() == _audio_ssrc)
			{
				clock_type = ClockType::AUDIO;
			}
			else
			{
				logtw("Could not find SSRC of sender report : %u", sr->GetSenderSsrc());
				return;
			}

			_lip_sync_clock->ApplySenderReportTime(clock_type, sr->GetMsw(), sr->GetLsw(), sr->GetTimestamp());
		}
	}

	// ov::Node Interface
	// RtpRtcp -> SRTP -> DTLS -> Edge(this)
	bool WebRTCStream::OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data)
	{
		if(ov::Node::GetNodeState() != ov::Node::NodeState::Started)
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
}