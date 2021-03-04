//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_stream.h"

namespace pvd
{
	std::shared_ptr<WebRTCStream> WebRTCStream::Create(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<PushProvider> &provider,
														const std::shared_ptr<const SessionDescription> &offer_sdp,
														const std::shared_ptr<const SessionDescription> &peer_sdp,
														const std::shared_ptr<IcePort> &ice_port)
	{
		auto stream = std::make_shared<WebRTCStream>(source_type, client_id, provider, offer_sdp, peer_sdp, ice_port);
		if(stream != nullptr)
		{
			if(stream->Start() == false)
			{
				return nullptr;
			}
		}
		return stream;
	}
	
	WebRTCStream::WebRTCStream(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<PushProvider> &provider,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp,
								const std::shared_ptr<IcePort> &ice_port)
		: PushStream(source_type, client_id, provider)
	{
		_offer_sdp = offer_sdp;
		_peer_sdp = peer_sdp;
		_ice_port = ice_port;
	}

	WebRTCStream::~WebRTCStream()
	{

	}

	bool WebRTCStream::Start()
	{
		

		return pvd::Stream::Start();
	}

	bool WebRTCStream::Stop()
	{
		return pvd::Stream::Stop();
	}

	bool WebRTCStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{

		return true;
	}
}