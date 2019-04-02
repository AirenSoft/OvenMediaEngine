//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <ice/ice.h>
#include <http_server/http_server.h>
#include <sdp/session_description.h>

class RtcIceCandidate;

class RtcSignallingObserver : public ov::EnableSharedFromThis<RtcSignallingObserver>
{
public:
	// Request offer가 오면, SDP를 생성해서 전달해줘야 함
	// observer가 여러 개 등록되어 있는 경우, 가장 먼저 반환되는 SDP를 사용함
	virtual std::shared_ptr<SessionDescription> OnRequestOffer(const ov::String &application_name, const ov::String &stream_name, std::vector<RtcIceCandidate> *ice_candidates) = 0;

	// remote SDP가 도착했을 때 호출되는 메서드
	virtual bool OnAddRemoteDescription(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &offer_sdp, const std::shared_ptr<SessionDescription> &peer_sdp) = 0;

	// client에서 ICE candidate가 도착했을 때 호출되는 메서드
	virtual bool OnIceCandidate(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<RtcIceCandidate> &candidate, const ov::String &username_fragment) = 0;

	// client에서 stop 이벤트가 도착했을 때 호출되는 메서드
    virtual bool OnStopCommand(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &offer_sdp, const std::shared_ptr<SessionDescription> &peer_sdp) = 0;

    // client bitrate info check method
    virtual uint32_t OnGetBitrate(const ov::String &application_name, const ov::String &stream_name) = 0;

};
