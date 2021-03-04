//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/provider/push_provider/stream.h"
#include "modules/ice/ice_port.h"
#include "modules/sdp/session_description.h"

namespace pvd
{
	class WebRTCStream : public pvd::PushStream
	{
	public:
		static std::shared_ptr<WebRTCStream> Create(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<PushProvider> &provider,
													const std::shared_ptr<const SessionDescription> &offer_sdp,
													const std::shared_ptr<const SessionDescription> &peer_sdp,
													const std::shared_ptr<IcePort> &ice_port);
		
		explicit WebRTCStream(StreamSourceType source_type, uint32_t client_id, const std::shared_ptr<PushProvider> &provider,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp,
								const std::shared_ptr<IcePort> &ice_port);
		~WebRTCStream() final;

		bool Start() override;
		bool Stop() override;

		// ------------------------------------------
		// Implementation of PushStream
		// ------------------------------------------
		PushStreamType GetPushStreamType() override
		{
			return PushStream::PushStreamType::DATA;
		}
		bool OnDataReceived(const std::shared_ptr<const ov::Data> &data) override;

	private:
		std::shared_ptr<const SessionDescription> _offer_sdp;
		std::shared_ptr<const SessionDescription> _peer_sdp;
		std::shared_ptr<IcePort> _ice_port;
	};
}