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

#include "modules/rtp_rtcp/rtcp_info/fir.h"

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
		: PushStream(source_type, stream_name, stream_id, provider)
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

		auto offer_media_desc_list = _offer_sdp->GetMediaList();
		auto peer_media_desc_list = _peer_sdp->GetMediaList();

		if(offer_media_desc_list.size() != peer_media_desc_list.size())
		{
			logte("m= line of answer does not correspond with offer");
			return false;
		}

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

				if(codec == PayloadAttr::SupportCodec::OPUS)
				{
					audio_track->SetCodecId(cmn::MediaCodecId::Opus);
				}
				else
				{
					logte("%s - Unsupported audio codec : %s", GetName().CStr(), first_payload->GetCodecParams().CStr());
					return false;
				}
				
				audio_track->SetId(first_payload->GetId());
				audio_track->SetMediaType(cmn::MediaType::Audio);
				audio_track->SetTimeBase(1, samplerate);
				audio_track->SetAudioTimestampScale(1.0);

				if (channels == 1)
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutMono);
				}
				else if (channels == 2)
				{
					audio_track->GetChannel().SetLayout(cmn::AudioChannel::Layout::LayoutStereo);
				}

				AddTrack(audio_track);
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

				auto video_track = std::make_shared<MediaTrack>();

				video_track->SetId(first_payload->GetId());
				video_track->SetMediaType(cmn::MediaType::Video);

				if(codec == PayloadAttr::SupportCodec::H264)
				{
					video_track->SetCodecId(cmn::MediaCodecId::H264);
				}
				else if(codec == PayloadAttr::SupportCodec::VP8)
				{
					video_track->SetCodecId(cmn::MediaCodecId::Vp8);
				}
				else
				{
					logte("%s - Unsupported video codec  : %s", GetName().CStr(), first_payload->GetCodecParams().CStr());
					return false;
				}

				video_track->SetTimeBase(1, timebase);
				video_track->SetVideoTimestampScale(1.0);

				if(AddDepacketizer(_video_payload_type, video_track->GetCodecId()) == false)
				{
					return false;
				}

				AddTrack(video_track);
			}
		}
		
		// Create Nodes
		_rtp_rtcp = std::make_shared<RtpRtcp>(RtpRtcpInterface::GetSharedPtr(), ssrc_list);

		_srtp_transport = std::make_shared<SrtpTransport>();
		_dtls_transport = std::make_shared<DtlsTransport>();

		auto application = std::static_pointer_cast<WebRTCApplication>(GetApplication());
		_dtls_transport->SetLocalCertificate(_certificate);
		_dtls_transport->StartDTLS();
		_dtls_ice_transport = std::make_shared<DtlsIceTransport>(GetId(), _ice_port);

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

		_fir_timer.Start();

		return pvd::Stream::Start();
	}

	bool WebRTCStream::AddDepacketizer(uint8_t payload_type, cmn::MediaCodecId codec_id)
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

		return pvd::Stream::Stop();
	}

	// From IcePort -> WebRTCProvider -> Application -> 
	bool WebRTCStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		logtd("OnDataReceived (%d)", data->GetLength());
		// To DTLS -> SRTP -> RTP|RTCP -> WebRTCStream::OnRtxpReceived
		
		//It must not be called during start and stop.
		std::shared_lock<std::shared_mutex> lock(_start_stop_lock);

		_dtls_ice_transport->OnDataReceived(NodeType::Edge, data);
		
		return true;
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets)
	{
		auto first_rtp_packet = rtp_packets.front();
		auto payload_type = first_rtp_packet->PayloadType();
		logtd("%s", first_rtp_packet->Dump().CStr());

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

		auto track = GetTrack(payload_type);

		auto frame = std::make_shared<MediaPacket>(track->GetMediaType(),
											  track->GetId(),
											  bitstream,
											  first_rtp_packet->Timestamp(),
											  first_rtp_packet->Timestamp(),
											  cmn::BitstreamFormat::H264_ANNEXB,
											 cmn::PacketType::NALU);

		logtd("Send Frame : track_id(%d) data_length(%d) pts(%d)",  track->GetId(), bitstream->GetLength(), first_rtp_packet->Timestamp());
		SendFrame(frame);

		// Send FIR to reduce keyframe interval
		if(_fir_timer.IsElapsed(1000))
		{
			_fir_timer.Update();
			SendFIR();
		}
		
		// Send Receiver Report
	}

	// From RtpRtcp node
	void WebRTCStream::OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info)
	{
		// Receive Sender Report

	}

	bool WebRTCStream::SendFIR()
	{
		FIR fir;
	
		fir.SetSrcSsrc(_video_ssrc);
		fir.AddFirMessage(_video_ssrc, _fir_seq++);

		auto rtcp_packet = std::make_shared<RtcpPacket>();
		rtcp_packet->Build(fir);

		_rtp_rtcp->SendData(NodeType::Rtcp, rtcp_packet->GetData());

		return true;
	}
}