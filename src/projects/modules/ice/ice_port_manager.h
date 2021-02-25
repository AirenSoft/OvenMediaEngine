//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_port.h"
#include "ice_port_observer.h"

#include <functional>
#include <memory>
#include <map>

#include <config/config.h>

class IcePortManager : public ov::Singleton<IcePortManager>
{
public:
	friend class ov::Singleton<IcePortManager>;

	~IcePortManager() override = default;

	std::shared_ptr<IcePort> CreatePort();
	bool CreateIceCandidates(const cfg::bind::pub::IceCandidates &ice_candidates);
	bool CreateTurnServer(uint16_t listening_port, const ov::SocketType socket_type);

	// TODO(Getroot): In the future, each IceCandidate and TurnServer can be released flexibly.
	// In the future, each IceCandidate and TurnServer can be released flexibly. 
	// Currently, WebRTC publisher and provider use IcePortManager until the server is shut down, 
	// so it is not a problem, but it is necessary if IcePortManager is used in dynamically 
	// created/deleted modules.
	bool Release();

protected:
	IcePortManager() = default;

	bool GenerateIceCandidates(const cfg::bind::pub::IceCandidates &ice_candidates, std::vector<RtcIceCandidate> *parsed_ice_candidate_list);
	bool ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, int *start_port, int *end_port);

private:
	
	std::shared_ptr<IcePort>	_ice_port = nullptr;
};
