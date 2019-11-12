#include "rtc_private.h"
#include "rtc_stream.h"
#include "rtc_application.h"
#include "rtc_session.h"

using namespace common;

std::shared_ptr<RtcStream> RtcStream::Create(const std::shared_ptr<Application> application,
                                             const StreamInfo &info,
                                             uint32_t worker_count)
{
	auto stream = std::make_shared<RtcStream>(application, info);
	if(!stream->Start(worker_count))
	{
		return nullptr;
	}
	return stream;
}

RtcStream::RtcStream(const std::shared_ptr<Application> application,
                     const StreamInfo &info)
	: Stream(application, info)
{
	_certificate = application->GetSharedPtrAs<RtcApplication>()->GetCertificate();
	_vp8_picture_id = 0x8000; // 1 {000 0000 0000 0000} 1 is marker for 15 bit length
}

RtcStream::~RtcStream()
{
	logtd("RtcStream(%d) has been terminated finally", GetId());
	Stop();
}

bool RtcStream::Start(uint32_t worker_count)
{
	// OFFER SDP 생성
	_offer_sdp = std::make_shared<SessionDescription>();
	_offer_sdp->SetOrigin("OvenMediaEngine", ov::Random::GenerateUInt32(), 2, "IN", 4, "127.0.0.1");
	_offer_sdp->SetTiming(0, 0);
	_offer_sdp->SetIceOption("trickle");
	_offer_sdp->SetIceUfrag(ov::Random::GenerateString(8));
	_offer_sdp->SetIcePwd(ov::Random::GenerateString(32));
	_offer_sdp->SetMsidSemantic("WMS", "*");
	_offer_sdp->SetFingerprint("sha-256", _certificate->GetFingerprint("sha-256"));

	// TODO(soulk): 현재는 Content가 Video 1개, Audio 1개라고 가정하고 개발되어 있다.
	// 현재의 Track은 Media Type과 Codec 정보만을 나타내며, Content를 구분할 수 없기 때문에
	// Multi Audio Channel 과 같은 내용을 표현할 수 없다.
	// GetContentCount() -> Content -> GetTrackCount로 확장해야 한다.
	// 다음은 위와 같은 제약사항으로 인해 개발된 임시 코드이다. by Getroot

	std::shared_ptr<MediaDescription> video_media_desc = nullptr;
	std::shared_ptr<MediaDescription> audio_media_desc = nullptr;

	bool first_video_desc = true;
	bool first_audio_desc = true;

	for(auto &track_item : _tracks)
	{
		ov::String codec = "";
		auto &track = track_item.second;

		switch(track->GetMediaType())
		{
			case MediaType::Video:
			{
				auto payload = std::make_shared<PayloadAttr>();

				switch(track->GetCodecId())
				{
					case MediaCodecId::Vp8:
						codec = "VP8";
						break;
					case MediaCodecId::H264:
						codec = "H264";

						payload->SetFmtp(ov::String::FormatString(
							// NonInterleaved => packetization-mode=1
							// baseline & lvl 3.1 => profile-level-id=42e01f
							"packetization-mode=1;profile-level-id=%x",
							0x42e01f
						));

						break;
					default:
						logtw("Unsupported codec(%d) is being input from media track", track->GetCodecId());
						continue;
				}

				if(first_video_desc)
				{
					video_media_desc = std::make_shared<MediaDescription>(_offer_sdp);
					video_media_desc->SetConnection(4, "0.0.0.0");
					// TODO(dimiden): Prevent duplication
					video_media_desc->SetMid(ov::Random::GenerateString(6));
					video_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
					video_media_desc->UseDtls(true);
					video_media_desc->UseRtcpMux(true);
					video_media_desc->SetDirection(MediaDescription::Direction::SendOnly);
					video_media_desc->SetMediaType(MediaDescription::MediaType::Video);
					video_media_desc->SetCname(ov::Random::GenerateUInt32(), ov::Random::GenerateString(16));

					_offer_sdp->AddMedia(video_media_desc);

					first_video_desc = false;
				}

				//TODO(getroot): WEBRTC에서는 TIMEBASE를 무조건 90000을 쓰는 것으로 보임, 정확히 알아볼것
				payload->SetRtpmap(track->GetId(), codec, 90000);

				video_media_desc->AddPayload(payload);

				// RTP Packetizer를 추가한다.
				AddPacketizer(false, payload->GetId(), video_media_desc->GetSsrc());

				break;
			}

			case MediaType::Audio:
			{
				auto payload = std::make_shared<PayloadAttr>();

				switch(track->GetCodecId())
				{
					case MediaCodecId::Opus:
						codec = "OPUS";

						// Enable inband-fec
						// a=fmtp:111 maxplaybackrate=16000; useinbandfec=1; maxaveragebitrate=20000
						 payload->SetFmtp("stereo=1;useinbandfec=1;");

						break;

					default:
						logtw("Unsupported codec(%d) is being input from media track", track->GetCodecId());
						continue;
				}

				if(first_audio_desc)
				{
					audio_media_desc = std::make_shared<MediaDescription>(_offer_sdp);
					audio_media_desc->SetConnection(4, "0.0.0.0");
					// TODO(dimiden): Need to prevent duplication
					audio_media_desc->SetMid(ov::Random::GenerateString(6));
					audio_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
					audio_media_desc->UseDtls(true);
					audio_media_desc->UseRtcpMux(true);
					audio_media_desc->SetDirection(MediaDescription::Direction::SendOnly);
					audio_media_desc->SetMediaType(MediaDescription::MediaType::Audio);
					audio_media_desc->SetCname(ov::Random::GenerateUInt32(), ov::Random::GenerateString(16));
					_offer_sdp->AddMedia(audio_media_desc);
					first_audio_desc = false;
				}

				// TODO(dimiden): Need to change to transcoding profile's bitrate and channel
				payload->SetRtpmap(track->GetId(), codec, 48000, "2");

				audio_media_desc->AddPayload(payload);

				// RTP Packetizer를 추가한다.
				AddPacketizer(true, payload->GetId(), audio_media_desc->GetSsrc());

				break;
			}

			default:
				// not supported type
				logtw("Not supported media type: %d", (int)(track->GetMediaType()));
				break;
		}
	}

	if (video_media_desc)
	{
        // RED & ULPFEC
        auto red_payload = std::make_shared<PayloadAttr>();
        red_payload->SetRtpmap(RED_PAYLOAD_TYPE, "red", 90000);
        auto ulpfec_payload = std::make_shared<PayloadAttr>();
        ulpfec_payload->SetRtpmap(ULPFEC_PAYLOAD_TYPE, "ulpfec", 90000);

        video_media_desc->AddPayload(red_payload);
        video_media_desc->AddPayload(ulpfec_payload);
    }

	ov::String offer_sdp_text;
	_offer_sdp->ToString(offer_sdp_text);

	logti("Stream is created : %s/%u", GetName().CStr(), GetId());

	return Stream::Start(worker_count);
}

bool RtcStream::Stop()
{
	_packetizers.clear();

	return Stream::Stop();
}

std::shared_ptr<SessionDescription> RtcStream::GetSessionDescription()
{
	return _offer_sdp;
}

bool RtcStream::OnRtpPacketized(std::shared_ptr<RtpPacket> packet)
{
	uint32_t rtp_payload_type = packet->PayloadType();
	uint32_t red_block_pt = 0;
	uint32_t origin_pt_of_fec = 0;

	if(rtp_payload_type == RED_PAYLOAD_TYPE)
	{
		red_block_pt = packet->Header()[packet->HeadersSize()-1];

		// RED includes FEC packet or Media packet.
		if(packet->IsUlpfec())
		{
			origin_pt_of_fec = packet->OriginPayloadType();
		}
	}

	// We make payload_type with the following structure:
	// 0               8                 16             24                 32
	//                 | origin_pt_of_fec | red block_pt | rtp_payload_type |
	uint32_t payload_type = rtp_payload_type | (red_block_pt << 8) | (origin_pt_of_fec << 16);
	BroadcastPacket(payload_type, packet->GetData());

	return true;
}

bool RtcStream::OnRtcpPacketized(std::shared_ptr<RtcpPacket> packet)
{
	return true;
}

void RtcStream::SendVideoFrame(std::shared_ptr<MediaTrack> track,
                               std::unique_ptr<EncodedFrame> encoded_frame,
                               std::unique_ptr<CodecSpecificInfo> codec_info,
                               std::unique_ptr<FragmentationHeader> fragmentation)
{
	// VideoFrame 데이터를 Protocol에 맞게 변환한다.

	// RTP Video Header를 생성한다. 사용한 CodecSpecificInfo는 버린다. (자동 삭제 됨)
	RTPVideoHeader rtp_video_header;
	memset(&rtp_video_header, 0, sizeof(RTPVideoHeader));

	if(codec_info)
	{
		MakeRtpVideoHeader(codec_info.get(), &rtp_video_header);
	}

	// RTP Packetizing
	// Track의 GetId와 PayloadType은 같다. Track의 ID로 Payload Type을 만들기 때문이다.
	auto packetizer = GetPacketizer(track->GetId());
	if(packetizer == nullptr)
	{
		return;
	}
	// RTP_SENDER에 등록된 RtpRtcpSession에 의해서 Packetizing이 완료되면 OnRtpPacketized 함수가 호출된다.
	packetizer->Packetize(encoded_frame->_frame_type,
	                      encoded_frame->_time_stamp,
	                      encoded_frame->_buffer->GetDataAs<uint8_t>(),
	                      encoded_frame->_length,
	                      fragmentation.get(),
	                      &rtp_video_header);
}

void RtcStream::SendAudioFrame(std::shared_ptr<MediaTrack> track,
                               std::unique_ptr<EncodedFrame> encoded_frame,
                               std::unique_ptr<CodecSpecificInfo> codec_info,
                               std::unique_ptr<FragmentationHeader> fragmentation)
{
	// AudioFrame 데이터를 Protocol에 맞게 변환한다.
	OV_ASSERT2(encoded_frame != nullptr);
	OV_ASSERT2(encoded_frame->_buffer != nullptr);

	// RTP Packetizing
	// Track의 GetId와 PayloadType은 같다. Track의 ID로 Payload Type을 만들기 때문이다.
	auto packetizer = GetPacketizer(track->GetId());
	if(packetizer == nullptr)
	{
		return;
	}

	packetizer->Packetize(encoded_frame->_frame_type,
	                      encoded_frame->_time_stamp,
	                      encoded_frame->_buffer->GetDataAs<uint8_t>(),
	                      encoded_frame->_length,
	                      fragmentation.get(),
	                      nullptr);
}

uint16_t RtcStream::AllocateVP8PictureID()
{
	_vp8_picture_id++;

	// PictureID is 7 bit or 15 bit. We use only 15 bit.
	if(_vp8_picture_id == 0)
	{
		// 1{000 0000 0000 0000} is initial number. (first bit means to use 15 bit size)
		_vp8_picture_id = 0x8000;
	}

	return _vp8_picture_id;
}

void RtcStream::MakeRtpVideoHeader(const CodecSpecificInfo *info, RTPVideoHeader *rtp_video_header)
{
	switch(info->codec_type)
	{
		case CodecType::Vp8:
			rtp_video_header->codec = RtpVideoCodecType::Vp8;
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
		case CodecType::H264:
			rtp_video_header->codec = RtpVideoCodecType::H264;
			rtp_video_header->codec_header.h264.packetization_mode = info->codec_specific.h264.packetization_mode;
			rtp_video_header->simulcast_idx = info->codec_specific.h264.simulcast_idx;
			return;

		case CodecType::Opus:
		case CodecType::Vp9:
		case CodecType::I420:
		case CodecType::Red:
		case CodecType::Ulpfec:
		case CodecType::Flexfec:
		case CodecType::Generic:
		case CodecType::Stereo:
		case CodecType::Unknown:
		default:
			break;
	}
}

void RtcStream::AddPacketizer(bool audio, uint8_t payload_type, uint32_t ssrc)
{
	auto packetizer = std::make_shared<RtpPacketizer>(audio, RtpRtcpPacketizerInterface::GetSharedPtr());
	packetizer->SetPayloadType(payload_type);
	packetizer->SetSSRC(ssrc);

	if(!audio)
	{
		packetizer->SetUlpfec(RED_PAYLOAD_TYPE, ULPFEC_PAYLOAD_TYPE);
	}

	_packetizers[payload_type] = packetizer;
}

std::shared_ptr<RtpPacketizer> RtcStream::GetPacketizer(uint8_t payload_type)
{
	if(!_packetizers.count(payload_type))
	{
		return nullptr;
	}

	return _packetizers[payload_type];
}
