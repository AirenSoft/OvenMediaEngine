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
#include <base/info/session.h>
#include <modules/sdp/session_description.h>
#include <modules/rtc_signalling/rtc_ice_candidate.h>

enum class IcePortConnectionState : int
{
	New,
	Checking,
	Connected,
	Completed,
	Failed,
	Disconnected,
	Closed,
	Max,
};

class IcePort;

class IcePortObserver : public ov::EnableSharedFromThis<IcePortObserver>
{
public:
	friend class IcePortManager;

	virtual void OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data)
	{
		// dummy function
	}
	virtual void OnRouteChanged(IcePort &port)
	{
		// dummy function
	}
	virtual void OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data) = 0;

private:
	uint32_t _id = 0;
	std::vector<RtcIceCandidate> _ice_candidate_list;
	ov::SocketType _turn_server_socket_type = ov::SocketType::Unknown;
	uint16_t _turn_server_port = 0;
};