
#include "rtc_stream.h"
#include "rtc_application.h"
#include "rtc_session.h"

std::shared_ptr<RtcStream> RtcStream::Create(const std::shared_ptr<Application> application,
											 const StreamInfo &info)
{
    auto stream = std::make_shared<RtcStream>(application, info);
    if(!stream->Start())
	{
		return nullptr;
	}
    return stream;
}

RtcStream::RtcStream(const std::shared_ptr<Application> application,
					 const StreamInfo &info)
    :Stream(application, info)
{
	_certificate = application->GetSharedPtrAs<RtcApplication>()->GetCertificate();
}

RtcStream::~RtcStream()
{
	logd("WEBRTC", "RtcStream(%d) has been terminated finally", GetId());
	Stop();
}

bool RtcStream::Start()
{
	// OFFER SDP 생성
	_offer_sdp = std::make_shared<SessionDescription>();
	_offer_sdp->SetOrigin("OvenMediaEngine", ov::Random::GenerateInteger(), 2, "IN", 4, "127.0.0.1");
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

	auto video_media_desc = std::make_shared<MediaDescription>(_offer_sdp);
	video_media_desc->SetConnection(4, "0.0.0.0");
	video_media_desc->SetMid(ov::Random::GenerateString(6));
	video_media_desc->SetSetup(MediaDescription::SetupType::ACTPASS);
	video_media_desc->UseDtls(true);
	video_media_desc->UseRtcpMux(true);
	video_media_desc->SetDirection(MediaDescription::Direction::SENDONLY);
	video_media_desc->SetMediaType(MediaDescription::MediaType::VIDEO);
	video_media_desc->SetCname(ov::Random::GenerateInteger(), ov::Random::GenerateString(16));

	auto audio_media_desc = std::make_shared<MediaDescription>(_offer_sdp);
	audio_media_desc->SetConnection(4, "0.0.0.0");
	audio_media_desc->SetMid(ov::Random::GenerateString(6));
	audio_media_desc->SetSetup(MediaDescription::SetupType::ACTPASS);
	audio_media_desc->UseDtls(true);
	audio_media_desc->UseRtcpMux(true);
	audio_media_desc->SetDirection(MediaDescription::Direction::SENDONLY);
	audio_media_desc->SetMediaType(MediaDescription::MediaType::VIDEO);
	audio_media_desc->SetCname(ov::Random::GenerateInteger(), ov::Random::GenerateString(16));

	for(int i=0; i<GetTrackCount(); i++)
	{
		auto track = GetTrackAt(i);

		if(track->GetMediaType() == MediaType::MEDIA_TYPE_VIDEO)
		{
			// Timebase
			track->GetTimeBase().GetDen(), track->GetTimeBase().GetNum();

			PayloadAttr::SupportCodec sdp_support_codec;

			switch(track->GetCodecId())
			{
				case MediaCodecId::CODEC_ID_VP8:
					sdp_support_codec = PayloadAttr::SupportCodec::VP8;
					break;
				case MediaCodecId ::CODEC_ID_H264:
					sdp_support_codec = PayloadAttr::SupportCodec::H264;
					break;
				default:
					logw("WEBRTC", "Unsupported codec(%d) is being input from media track",
						 track->GetCodecId());
					continue;
					break;
			}

			auto payload = std::make_shared<PayloadAttr>();
			//TODO(getroot): WEBRTC에서는 TIMEBASE를 무조건 90000을 쓰는 것으로 보임, 정확히 알아볼것
			payload->SetRtpmap(sdp_support_codec, 90000);

			video_media_desc->AddPayload(payload);

			// Webrtc에서 사용하는 Track만 별도로 관리한다.
			AddRtcTrack(payload->GetId(), track);
		}
		else if(track->GetMediaType() == MediaType::MEDIA_TYPE_AUDIO)
		{
			// TODO(getroot): Audio 개발하기
			logw("WEBRTC", "Unsupported audio codec(%d) is being input from media track", track->GetCodecId());
		}
		else
		{
			// not support yet
			continue;
		}

	}

	// Media Description 연결
	_offer_sdp->AddMedia(video_media_desc);

	ov::String offer_sdp_text;
	_offer_sdp->ToString(offer_sdp_text);

	logi("WEBRTC", "Stream created : %s/%u", GetName().CStr(), GetId());
	logd("WEBRTC", "%s", offer_sdp_text.CStr());

	return Stream::Start();
}

bool RtcStream::Stop()
{
	_rtc_track.clear();

	return Stream::Stop();
}

std::shared_ptr<SessionDescription> RtcStream::GetSessionDescription()
{
	return _offer_sdp;
}

std::shared_ptr<RtcSession> RtcStream::FindRtcSessionByPeerSDPSessionID(uint32_t session_id)
{
	// 모든 Session에 Frame을 전달한다.
	for (auto const &x : GetSessionMap())
	{
		auto session = std::static_pointer_cast<RtcSession>(x.second);

		if(session->GetPeerSDP() != nullptr && session->GetPeerSDP()->GetSessionId() == session_id)
		{
			return session;
		}
	}

	return nullptr;
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

	// 모든 Session에 Frame을 전달한다.
	for(auto const& x : GetSessionMap())
	{
		auto session = x.second;

		// RTP_RTCP는 Blocking 방식의 Packetizer이므로
		// shared_ptr, unique_ptr을 사용하지 않아도 무방하다. 사실 다 고치기가 귀찮음 ㅠㅠ
		// TODO(getroot): 향후 Performance 증가를 위해 RTP Packetize를 한번 하고 모든 Session에 전달하는 방법을
		// 실험해보고 성능이 좋아지면 적용한다.
		std::static_pointer_cast<RtcSession>(session)->SendOutgoingVideoData(track,
																			 encoded_frame->_frameType,
																			  encoded_frame->_timeStamp,
																			  encoded_frame->_buffer,
																			  encoded_frame->_length,
																			  fragmentation.get(),
																			  &rtp_video_header);
	}
	// TODO(getroot): 향후 ov::Data로 변경한다.
	delete[] encoded_frame->_buffer;
}

void RtcStream::MakeRtpVideoHeader(const CodecSpecificInfo* info, RTPVideoHeader *rtp_video_header)
{
	switch(info->codecType)
	{
		case kVideoCodecVP8:
			rtp_video_header->codec = kRtpVideoVp8;
			rtp_video_header->codecHeader.VP8.InitRTPVideoHeaderVP8();
			rtp_video_header->codecHeader.VP8.pictureId = info->codecSpecific.VP8.pictureId;
			rtp_video_header->codecHeader.VP8.nonReference = info->codecSpecific.VP8.nonReference;
			rtp_video_header->codecHeader.VP8.temporalIdx = info->codecSpecific.VP8.temporalIdx;
			rtp_video_header->codecHeader.VP8.layerSync = info->codecSpecific.VP8.layerSync;
			rtp_video_header->codecHeader.VP8.tl0PicIdx = info->codecSpecific.VP8.tl0PicIdx;
			rtp_video_header->codecHeader.VP8.keyIdx = info->codecSpecific.VP8.keyIdx;
			rtp_video_header->simulcastIdx = info->codecSpecific.VP8.simulcastIdx;
			return;
	}
}

void RtcStream::AddRtcTrack(uint32_t payload_type, std::shared_ptr<MediaTrack> track)
{
	_rtc_track[payload_type] = track;
}

std::shared_ptr<MediaTrack>	RtcStream::GetRtcTrack(uint32_t payload_type)
{
	if(!_rtc_track.count(payload_type))
	{
		return nullptr;
	}

	return _rtc_track[payload_type];
}