//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/http_server/http_server.h>
#include <modules/ice/ice.h>
#include <modules/sdp/session_description.h>

#include <memory>

class RtcIceCandidate;
class WebSocketClient;

class RtcSignallingObserver : public ov::EnableSharedFromThis<RtcSignallingObserver>
{
public:
	// When request offer is requested, SDP of OME must be created and returned
	// If there are multiple Observer registered, the SDP returned first is used
	virtual std::shared_ptr<const SessionDescription> OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client,
																	 const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
																	 std::vector<RtcIceCandidate> *ice_candidates) = 0;

	// A callback called when remote SDP arrives
	virtual bool OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
										const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
										const std::shared_ptr<const SessionDescription> &offer_sdp,
										const std::shared_ptr<const SessionDescription> &peer_sdp) = 0;

	// A callback called when client ICE candidates arrive
	virtual bool OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<RtcIceCandidate> &candidate,
								const ov::String &username_fragment) = 0;

	// A callback called when client sent stop event
	virtual bool OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
							   const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
							   const std::shared_ptr<const SessionDescription> &offer_sdp,
							   const std::shared_ptr<const SessionDescription> &peer_sdp) = 0;

	// A callback called when client requests bitrate information (currently NOT USED)
	virtual uint32_t OnGetBitrate(const std::shared_ptr<WebSocketClient> &ws_client,
								  const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name) = 0;
};
