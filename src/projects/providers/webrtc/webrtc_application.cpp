//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_private.h"
#include "webrtc_application.h"

namespace pvd
{
	std::shared_ptr<WebRTCApplication> WebRTCApplication::Create(const std::shared_ptr<pvd::PushProvider> &provider, const info::Application &application_info, const std::shared_ptr<IcePort> &ice_port, const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
	{
		auto application = std::make_shared<WebRTCApplication>(provider, application_info, ice_port, rtc_signalling);
		if(application->Start() == false)
		{
			return nullptr;
		}
		return application;
	}

	WebRTCApplication::WebRTCApplication(const std::shared_ptr<pvd::PushProvider> &provider, const info::Application &info, const std::shared_ptr<IcePort> &ice_port, const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
		: PushApplication(provider, info)
	{
		_ice_port = ice_port;
		_rtc_signalling = rtc_signalling;
	}

	bool WebRTCApplication::Start()
	{
		_certificate = CreateCertificate();
		_offer_sdp = CreateOfferSDP(_certificate);

		if(_certificate == nullptr || _offer_sdp == nullptr)
		{
			logte("Failed to generate certificate and offer SDP. Could not start %s/s", GetApplicationTypeName(), GetName().CStr());
			return false;
		}

		return Application::Start();
	}

	bool WebRTCApplication::Stop()
	{
		return Application::Stop();
	}

	std::shared_ptr<Certificate> WebRTCApplication::CreateCertificate()
	{
		auto certificate = std::make_shared<Certificate>();

		auto error = certificate->Generate();
		if(error != nullptr)
		{
			logte("Cannot create certificate: %s", error->ToString().CStr());
			return nullptr;
		}

		return certificate;
	}

	std::shared_ptr<const SessionDescription> WebRTCApplication::GetOfferSDP()
	{
		return _offer_sdp;
	}

	std::shared_ptr<SessionDescription> WebRTCApplication::CreateOfferSDP(const std::shared_ptr<Certificate> &certificate)
	{
		if(certificate == nullptr)
		{
			return nullptr;
		}

		//TODO(Getroot): Apply configuration

		auto offer_sdp = std::make_shared<SessionDescription>();
		offer_sdp->SetOrigin("OvenMediaEngine", ov::Random::GenerateUInt32(), 2, "IN", 4, "127.0.0.1");
		offer_sdp->SetTiming(0, 0);
		offer_sdp->SetIceOption("trickle");
		offer_sdp->SetIceUfrag(ov::Random::GenerateString(8));
		offer_sdp->SetIcePwd(ov::Random::GenerateString(32));

		// MSID
		auto msid = ov::Random::GenerateString(36);
		offer_sdp->SetMsidSemantic("WMS", msid);
		offer_sdp->SetFingerprint("sha-256", certificate->GetFingerprint("sha-256"));

		uint8_t payload_type_num = 100;

		auto cname = ov::Random::GenerateString(16);

		///////////////////////////////////////
		// Video Media Description
		///////////////////////////////////////
		auto video_media_desc = std::make_shared<MediaDescription>();
		video_media_desc->SetMediaType(MediaDescription::MediaType::Video);
		video_media_desc->SetCname(cname);
		video_media_desc->SetConnection(4, "0.0.0.0");
		video_media_desc->SetMid(ov::Random::GenerateString(6));
		video_media_desc->SetMsid(msid, ov::Random::GenerateString(36));
		video_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
		video_media_desc->UseDtls(true);
		video_media_desc->UseRtcpMux(true);
		video_media_desc->SetDirection(MediaDescription::Direction::RecvOnly);
		video_media_desc->SetSsrc(ov::Random::GenerateUInt32());
		// not support yet
		// video_media_desc->SetRtxSsrc(ov::Random::GenerateUInt32()); 

		// VP8
		auto payload = std::make_shared<PayloadAttr>();
		payload->SetRtpmap(payload_type_num++, "VP8", 90000);
		video_media_desc->AddPayload(payload);

		// H264
		payload = std::make_shared<PayloadAttr>();
		payload->SetRtpmap(payload_type_num++, "H264", 90000);
		payload->SetFmtp(ov::String::FormatString("packetization-mode=1;profile-level-id=%x;level-asymmetry-allowed=1",	0x42e01f));
		video_media_desc->AddPayload(payload);

		video_media_desc->Update();
		offer_sdp->AddMedia(video_media_desc);

		///////////////////////////////////////
		// Audio Media Description
		///////////////////////////////////////
		auto audio_media_desc = std::make_shared<MediaDescription>();
		audio_media_desc->SetConnection(4, "0.0.0.0");
		// TODO(dimiden): Need to prevent duplication
		audio_media_desc->SetMid(ov::Random::GenerateString(6));
		audio_media_desc->SetMsid(msid, ov::Random::GenerateString(36));
		audio_media_desc->SetSetup(MediaDescription::SetupType::ActPass);
		audio_media_desc->UseDtls(true);
		audio_media_desc->UseRtcpMux(true);
		audio_media_desc->SetDirection(MediaDescription::Direction::RecvOnly);
		audio_media_desc->SetMediaType(MediaDescription::MediaType::Audio);
		audio_media_desc->SetCname(cname);
		audio_media_desc->SetSsrc(ov::Random::GenerateUInt32());

		// OPUS
		payload = std::make_shared<PayloadAttr>();
		payload->SetFmtp("sprop-stereo=1;stereo=1;minptime=10;useinbandfec=1");
		payload->SetRtpmap(payload_type_num++, "OPUS", 48000, "2");

		audio_media_desc->Update();
		offer_sdp->AddMedia(audio_media_desc);

		offer_sdp->Update();
		logtd("Offer SDP created : %s", offer_sdp->ToString().CStr());

		return offer_sdp;
	}
}