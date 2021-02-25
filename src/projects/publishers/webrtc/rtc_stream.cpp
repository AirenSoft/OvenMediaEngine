#include "rtc_stream.h"

#include <base/info/media_extradata.h>
#include <modules/bitstream/h264/h264_decoder_configuration_record.h>

#include "rtc_application.h"
#include "rtc_private.h"
#include "rtc_session.h"

using namespace cmn;

/***************************
 SDP Sample
****************************
v=0
o=OvenMediaEngine 101 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE jyJ5Pe 7LdTsW
a=group:LS jyJ5Pe 7LdTsW
a=msid-semantic:WMS 0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf
a=fingerprint:sha-256 9A:F5:91:C4:C8:AD:9B:FB:95:5F:2E:30:49:E6:98:EC:63:BF:B0:15:26:DF:B7:E9:5F:9F:6C:C9:90:6F:0B:F4
a=ice-options:trickle
a=ice-ufrag:Xnh541
a=ice-pwd:fR9dQgrLynGWq3iF07teYKu2STHJPIkM
m=video 9 UDP/TLS/RTP/SAVPF 100 101 120 121 122
c=IN IP4 0.0.0.0
a=sendonly
a=mid:jyJ5Pe
a=setup:actpass
a=rtcp-mux
a=msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf 6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
a=rtpmap:100 H264/90000
a=fmtp:100 packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1
a=rtcp-fb:100 nack
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:120 red/90000
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=120
a=rtpmap:122 ulpfec/90000
a=ssrc-group:FID 2808715097 1263422112
a=ssrc:2808715097 cname:A9KW3tqkuJhs25BN
a=ssrc:2808715097 msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf 6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
a=ssrc:2808715097 mslabel:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf
a=ssrc:2808715097 label:6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
a=ssrc:1263422112 cname:A9KW3tqkuJhs25BN
a=ssrc:1263422112 msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf 6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
a=ssrc:1263422112 mslabel:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf
a=ssrc:1263422112 label:6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
m=audio 9 UDP/TLS/RTP/SAVPF 102
c=IN IP4 0.0.0.0
a=sendonly
a=mid:7LdTsW
a=setup:actpass
a=rtcp-mux
a=msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf X6EozKm0lj57uafc3JW2sOven1Sp9RMFY8kB
a=rtpmap:102 OPUS/48000/2
a=fmtp:102 stereo=1;useinbandfec=1;
a=ssrc:1049140135 cname:A9KW3tqkuJhs25BN
a=ssrc:1049140135 msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf X6EozKm0lj57uafc3JW2sOven1Sp9RMFY8kB
a=ssrc:1049140135 mslabel:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf
a=ssrc:1049140135 label:X6EozKm0lj57uafc3JW2sOven1Sp9RMFY8kB

****************************/

std::shared_ptr<RtcStream> RtcStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info,
											 uint32_t worker_count)
{
	auto stream = std::make_shared<RtcStream>(application, info, worker_count);
	return stream;
}

RtcStream::RtcStream(const std::shared_ptr<pub::Application> application,
					 const info::Stream &info,
					 uint32_t worker_count)
	: Stream(application, info),
	_worker_count(worker_count)
{
	_certificate = application->GetSharedPtrAs<RtcApplication>()->GetCertificate();
	_vp8_picture_id = 0x8000;  // 1 {000 0000 0000 0000} 1 is marker for 15 bit length
}

RtcStream::~RtcStream()
{
	logtd("RtcStream(%d) has been terminated finally", GetId());
	Stop();
}

bool RtcStream::Start()
{
	if(GetState() != State::CREATED)
	{
		return false;
	}

	if (!CreateStreamWorker(_worker_count))
	{
		return false;
	}

	_rtx_enabled = GetApplicationInfo().GetConfig().GetPublishers().GetWebrtcPublisher().IsRtxEnabled();
	_ulpfec_enabled = GetApplicationInfo().GetConfig().GetPublishers().GetWebrtcPublisher().IsUlpfecEnalbed();

	_offer_sdp = std::make_shared<SessionDescription>();
	_offer_sdp->SetOrigin("OvenMediaEngine", ov::Random::GenerateUInt32(), 2, "IN", 4, "127.0.0.1");
	_offer_sdp->SetTiming(0, 0);
	_offer_sdp->SetIceOption("trickle");
	_offer_sdp->SetIceUfrag(ov::Random::GenerateString(8));
	_offer_sdp->SetIcePwd(ov::Random::GenerateString(32));

	// MSID
	auto msid = ov::Random::GenerateString(36);
	_offer_sdp->SetMsidSemantic("WMS", msid);
	_offer_sdp->SetFingerprint("sha-256", _certificate->GetFingerprint("sha-256"));

	std::shared_ptr<MediaDescription> video_media_desc = nullptr;
	std::shared_ptr<MediaDescription> audio_media_desc = nullptr;

	bool first_video_desc = true;
	bool first_audio_desc = true;
	uint8_t payload_type_num = PAYLOAD_TYPE_OFFSET;

	auto cname = ov::Random::GenerateString(16);

	for (auto &track_item : _tracks)
	{
		ov::String codec = "";
		auto &track = track_item.second;

		switch (track->GetMediaType())
		{
			case MediaType::Video: {
				auto payload = std::make_shared<PayloadAttr>();

				switch (track->GetCodecId())
				{
					case MediaCodecId::Vp8:
						codec = "VP8";
						break;
					case MediaCodecId::H265:
						codec = "H265";
						break;
					case MediaCodecId::H264:
						codec = "H264";

						{
							const auto &codec_extradata = track_item.second->GetCodecExtradata();

							//(NOTE) The software decoder of Firefox or Chrome cannot play when 64001f (High, 3.1) stream is input. 
							// However, when I put the fake information of 42e01f in FMTP, I confirmed that both Firefox and Chrome play well (high profile, but stream without B-Frame). 
							// I thought it would be better to put 42e01f in fmtp than put the correct value, so I decided to put fake information.

							AVCDecoderConfigurationRecord config;
							if (0 && codec_extradata.size() > 0 && AVCDecoderConfigurationRecord::Parse(codec_extradata.data(), codec_extradata.size(), config) == true)
							{
								// profile-level-id
								// |profile idc|contraint flag|level idc|
								auto h264_fmtp = ov::String::FormatString("packetization-mode=1;profile-level-id=%02x%02x%02x;level-asymmetry-allowed=1",
														config.ProfileIndication(), config.Compatibility(), config.LevelIndication());
								logtc("FMTP : %s", h264_fmtp.CStr());
								payload->SetFmtp(h264_fmtp);
							}
							else
							{
								payload->SetFmtp(ov::String::FormatString(
									// NonInterleaved => packetization-mode=1
									// baseline & lvl 3.1 => profile-level-id=42e01f
									"packetization-mode=1;profile-level-id=%x;level-asymmetry-allowed=1",
									0x42e01f));
							}
						}

						break;
					default:
						logti("Unsupported codec(%s/%s) is being input from media track",
							  ::StringFromMediaType(track->GetMediaType()).CStr(),
							  ::StringFromMediaCodecId(track->GetCodecId()).CStr());
						continue;
				}

				if (first_video_desc)
				{
					video_media_desc = std::make_shared<MediaDescription>();
					video_media_desc->SetConnection(4, "0.0.0.0");
					video_media_desc->SetMid(ov::Random::GenerateString(6));
					video_media_desc->SetMsid(msid, ov::Random::GenerateString(36));
					/*
					https://tools.ietf.org/html/rfc5763#section-5
					
					The endpoint MUST use the setup attribute defined in [RFC4145].
					The endpoint that is the offerer MUST use the setup attribute
					value of setup:actpass and be prepared to receive a client_hello
					before it receives the answer.  The answerer MUST use either a
					setup attribute value of setup:active or setup:passive.  Note that
					if the answerer uses setup:passive, then the DTLS handshake will
					not begin until the answerer is received, which adds additional
					latency. setup:active allows the answer and the DTLS handshake to
					occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
					party is active MUST initiate a DTLS handshake by sending a
					ClientHello over each flow (host/port quartet).
					*/
					video_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
					video_media_desc->UseDtls(true);
					video_media_desc->UseRtcpMux(true);
					video_media_desc->SetDirection(MediaDescription::Direction::SendOnly);
					video_media_desc->SetMediaType(MediaDescription::MediaType::Video);
					// Cname
					video_media_desc->SetCname(cname);
					// Media SSRC
					video_media_desc->SetSsrc(ov::Random::GenerateUInt32());
					// RTX SSRC
					if (_rtx_enabled == true)
					{
						video_media_desc->SetRtxSsrc(ov::Random::GenerateUInt32());
					}
					_offer_sdp->AddMedia(video_media_desc);
					first_video_desc = false;
				}

				payload->SetRtpmap(payload_type_num++, codec, 90000);

				if (_rtx_enabled == true)
				{
					payload->EnableRtcpFb(PayloadAttr::RtcpFbType::Nack, true);
				}

				video_media_desc->AddPayload(payload);

				// For RTX
				if (_rtx_enabled == true)
				{
					auto rtx_payload = std::make_shared<PayloadAttr>();
					rtx_payload->SetRtpmap(payload_type_num++, "rtx", 90000);
					rtx_payload->SetFmtp(ov::String::FormatString("apt=%d", payload->GetId()));
					video_media_desc->AddPayload(rtx_payload);
					AddRtpHistory(payload->GetId(), rtx_payload->GetId(), video_media_desc->GetRtxSsrc());
				}

				video_media_desc->Update();
				AddPacketizer(track->GetCodecId(), track->GetId(), payload->GetId(), video_media_desc->GetSsrc());
				break;
			}

			case MediaType::Audio: {
				auto payload = std::make_shared<PayloadAttr>();

				switch (track->GetCodecId())
				{
					case MediaCodecId::Opus:
						codec = "OPUS";

						// Enable inband-fec
						// a=fmtp:111 maxplaybackrate=16000; useinbandfec=1; maxaveragebitrate=20000
						if (track->GetChannel().GetLayout() == cmn::AudioChannel::Layout::LayoutStereo)
						{
							payload->SetFmtp("sprop-stereo=1;stereo=1;minptime=10;useinbandfec=1");
						}
						else
						{
							payload->SetFmtp("minptime=10;useinbandfec=1");
						}
						break;

					default:
						logti("Unsupported codec(%s/%s) is being input from media track",
							  ::StringFromMediaType(track->GetMediaType()).CStr(),
							  ::StringFromMediaCodecId(track->GetCodecId()).CStr());
						continue;
				}

				if (first_audio_desc)
				{
					audio_media_desc = std::make_shared<MediaDescription>();
					audio_media_desc->SetConnection(4, "0.0.0.0");
					// TODO(dimiden): Need to prevent duplication
					audio_media_desc->SetMid(ov::Random::GenerateString(6));
					audio_media_desc->SetMsid(msid, ov::Random::GenerateString(36));
					audio_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
					audio_media_desc->UseDtls(true);
					audio_media_desc->UseRtcpMux(true);
					audio_media_desc->SetDirection(MediaDescription::Direction::SendOnly);
					audio_media_desc->SetMediaType(MediaDescription::MediaType::Audio);
					// Cname
					audio_media_desc->SetCname(cname);
					// Media SSRC
					audio_media_desc->SetSsrc(ov::Random::GenerateUInt32());
					_offer_sdp->AddMedia(audio_media_desc);
					first_audio_desc = false;
				}

				payload->SetRtpmap(payload_type_num++, codec, static_cast<uint32_t>(track->GetSample().GetRateNum()),
								   std::to_string(track->GetChannel().GetCounts()).c_str());

				audio_media_desc->AddPayload(payload);
				audio_media_desc->Update();

				AddPacketizer(track->GetCodecId(), track->GetId(), payload->GetId(), audio_media_desc->GetSsrc());

				break;
			}

			default:
				// not supported type
				logtw("Not supported media type: %d", (int)(track->GetMediaType()));
				break;
		}
	}

	if (video_media_desc && _ulpfec_enabled == true)
	{
		// RED & ULPFEC
		auto red_payload = std::make_shared<PayloadAttr>();
		red_payload->SetRtpmap(RED_PAYLOAD_TYPE, "red", 90000);
		if (_rtx_enabled == true)
		{
			red_payload->EnableRtcpFb(PayloadAttr::RtcpFbType::Nack, true);
		}
		video_media_desc->AddPayload(red_payload);

		// ULPFEC
		auto ulpfec_payload = std::make_shared<PayloadAttr>();
		ulpfec_payload->SetRtpmap(ULPFEC_PAYLOAD_TYPE, "ulpfec", 90000);
		video_media_desc->AddPayload(ulpfec_payload);

		// For RTX
		if (_rtx_enabled == true)
		{
			// RTX for RED
			auto rtx_payload = std::make_shared<PayloadAttr>();
			rtx_payload->SetRtpmap(RED_RTX_PAYLOAD_TYPE, "rtx", 90000);
			rtx_payload->SetFmtp(ov::String::FormatString("apt=%d", RED_PAYLOAD_TYPE));

			AddRtpHistory(red_payload->GetId(), rtx_payload->GetId(), video_media_desc->GetRtxSsrc());

			video_media_desc->AddPayload(rtx_payload);
		}

		video_media_desc->Update();
	}

	logtd("Stream is created : %s/%u", GetName().CStr(), GetId());
	_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(pub::Stream::GetSharedPtr()));
	_offer_sdp->Update();

	logtd("%s", _offer_sdp->ToString().CStr());

	return Stream::Start();
}

bool RtcStream::Stop()
{
	if(GetState() != State::STARTED)
	{
		return false;
	}

	_offer_sdp->Release();

	std::lock_guard<std::shared_mutex> lock(_packetizers_lock);
	_packetizers.clear();

	return Stream::Stop();
}

std::shared_ptr<SessionDescription> RtcStream::GetSessionDescription()
{
	if(GetState() != State::STARTED)
	{
		return nullptr;
	}

	return _offer_sdp;
}

bool RtcStream::OnRtpPacketized(std::shared_ptr<RtpPacket> packet)
{
	auto stream_packet = std::make_any<std::shared_ptr<RtpPacket>>(packet);
	BroadcastPacket(stream_packet);

	if (_stream_metrics != nullptr)
	{
		_stream_metrics->IncreaseBytesOut(PublisherType::Webrtc, packet->GetData()->GetLength() * GetSessionCount());
	}

	if (_rtx_enabled == true)
	{
		// Store for retransmission
		auto history = GetHistory(packet->PayloadType());
		if (history != nullptr)
		{
			history->StoreRtpPacket(packet);
		}
	}

	return true;
}

void RtcStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != State::STARTED)
	{
		return;
	}

	auto media_track = GetTrack(media_packet->GetTrackId());

	// Create RTP Video Header
	CodecSpecificInfo codec_info;
	RTPVideoHeader rtp_video_header;

	codec_info.codec_type = media_track->GetCodecId();

	if (codec_info.codec_type == MediaCodecId::Vp8)
	{
		// Structure for future expansion.
		// In the future, when OME uses codec-specific features, certain information is obtained from media_packet.
		codec_info.codec_specific.vp8 = CodecSpecificInfoVp8();
	}
	else if (codec_info.codec_type == MediaCodecId::H264 ||
			 codec_info.codec_type == MediaCodecId::H265)
	{
		codec_info.codec_specific.h26X = CodecSpecificInfoH26X();
	}

	memset(&rtp_video_header, 0, sizeof(RTPVideoHeader));

	MakeRtpVideoHeader(&codec_info, &rtp_video_header);

	// RTP Packetizing
	auto packetizer = GetPacketizer(media_track->GetId());
	if (packetizer == nullptr)
	{
		return;
	}

	auto frame_type = (media_packet->GetFlag() == MediaPacketFlag::Key) ? FrameType::VideoFrameKey : FrameType::VideoFrameDelta;
	auto timestamp = media_packet->GetPts();
	auto data = media_packet->GetData();
	auto fragmentation = media_packet->GetFragHeader();

	packetizer->Packetize(frame_type,
						  timestamp,
						  data->GetDataAs<uint8_t>(),
						  data->GetLength(),
						  fragmentation,
						  &rtp_video_header);
}

void RtcStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != State::STARTED)
	{
		return;
	}

	auto media_track = GetTrack(media_packet->GetTrackId());

	// RTP Packetizing
	auto packetizer = GetPacketizer(media_track->GetId());
	if (packetizer == nullptr)
	{
		return;
	}

	auto frame_type = (media_packet->GetFlag() == MediaPacketFlag::Key) ? FrameType::AudioFrameKey : FrameType::AudioFrameDelta;
	auto timestamp = media_packet->GetPts();
	auto data = media_packet->GetData();
	auto fragmentation = media_packet->GetFragHeader();

	packetizer->Packetize(frame_type,
						  timestamp,
						  data->GetDataAs<uint8_t>(),
						  data->GetLength(),
						  fragmentation,
						  nullptr);
}

uint16_t RtcStream::AllocateVP8PictureID()
{
	_vp8_picture_id++;

	// PictureID is 7 bit or 15 bit. We use only 15 bit.
	if (_vp8_picture_id == 0)
	{
		// 1{000 0000 0000 0000} is initial number. (first bit means to use 15 bit size)
		_vp8_picture_id = 0x8000;
	}

	return _vp8_picture_id;
}

void RtcStream::MakeRtpVideoHeader(const CodecSpecificInfo *info, RTPVideoHeader *rtp_video_header)
{
	switch (info->codec_type)
	{
		case cmn::MediaCodecId::Vp8:
			rtp_video_header->codec = cmn::MediaCodecId::Vp8;
			rtp_video_header->codec_header.vp8.InitRTPVideoHeaderVP8();
			// With Ulpfec, picture id is needed.
			rtp_video_header->codec_header.vp8.picture_id = AllocateVP8PictureID();
			rtp_video_header->codec_header.vp8.non_reference = info->codec_specific.vp8.non_reference;
			rtp_video_header->codec_header.vp8.temporal_idx = info->codec_specific.vp8.temporal_idx;
			rtp_video_header->codec_header.vp8.layer_sync = info->codec_specific.vp8.layer_sync;
			rtp_video_header->codec_header.vp8.tl0_pic_idx = info->codec_specific.vp8.tl0_pic_idx;
			rtp_video_header->codec_header.vp8.key_idx = info->codec_specific.vp8.key_idx;
			rtp_video_header->simulcast_idx = info->codec_specific.vp8.simulcast_idx;
			return;
		case cmn::MediaCodecId::H264:
			rtp_video_header->codec = cmn::MediaCodecId::H264;
			rtp_video_header->codec_header.h26X.packetization_mode = info->codec_specific.h26X.packetization_mode;
			rtp_video_header->simulcast_idx = info->codec_specific.h26X.simulcast_idx;
			return;

		case cmn::MediaCodecId::H265:
			rtp_video_header->codec = cmn::MediaCodecId::H265;
			rtp_video_header->codec_header.h26X.packetization_mode = info->codec_specific.h26X.packetization_mode;
			rtp_video_header->simulcast_idx = info->codec_specific.h26X.simulcast_idx;
			return;
		default:
			break;
	}
}

void RtcStream::AddPacketizer(cmn::MediaCodecId codec_id, uint32_t id, uint8_t payload_type, uint32_t ssrc)
{
	logtd("Add Packetizer : codec(%u) id(%u) pt(%d) ssrc(%u)", codec_id, id, payload_type, ssrc);

	auto packetizer = std::make_shared<RtpPacketizer>(RtpRtcpPacketizerInterface::GetSharedPtr());
	packetizer->SetPayloadType(payload_type);
	packetizer->SetSSRC(ssrc);

	switch (codec_id)
	{
		case MediaCodecId::Vp8:
		case MediaCodecId::H264:
		case MediaCodecId::H265: {
			packetizer->SetVideoCodec(codec_id);
			if (_ulpfec_enabled == true)
			{
				packetizer->SetUlpfec(RED_PAYLOAD_TYPE, ULPFEC_PAYLOAD_TYPE);
			}
			break;
		}
		case MediaCodecId::Opus:
			packetizer->SetAudioCodec(codec_id);
			break;
		default:
			// No support codecs
			return;
	}

	std::lock_guard<std::shared_mutex> lock(_packetizers_lock);
	_packetizers[id] = packetizer;
}

std::shared_ptr<RtpPacketizer> RtcStream::GetPacketizer(uint32_t id)
{
	std::shared_lock<std::shared_mutex> lock(_packetizers_lock);
	if (!_packetizers.count(id))
	{
		return nullptr;
	}

	return _packetizers[id];
}

void RtcStream::AddRtpHistory(uint8_t origin_payload_type, uint8_t rtx_payload_type, uint32_t rtx_ssrc)
{
	auto history = std::make_shared<RtpHistory>(origin_payload_type, rtx_payload_type, rtx_ssrc);
	_rtp_history_map[origin_payload_type] = history;
}

std::shared_ptr<RtpHistory> RtcStream::GetHistory(uint8_t origin_payload_type)
{
	if (!_rtp_history_map.count(origin_payload_type))
	{
		return nullptr;
	}

	return _rtp_history_map[origin_payload_type];
}

std::shared_ptr<RtxRtpPacket> RtcStream::GetRtxRtpPacket(uint8_t origin_payload_type, uint16_t origin_sequence_number)
{
	if(GetState() != State::STARTED)
	{
		return nullptr;
	}

	auto history = GetHistory(origin_payload_type);
	if (history == nullptr)
	{
		return nullptr;
	}

	return history->GetRtxRtpPacket(origin_sequence_number);
}