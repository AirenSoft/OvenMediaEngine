//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtc_p2p_manager.h"

peer_id_t RtcP2PManager::FindHost(const std::shared_ptr<WebSocketClient> &response)
{
	auto user_agent = response->GetRequest()->GetHeader("USER-AGENT");

	if(user_agent.IsEmpty() == false)
	{
		auto result = ParseUserAgent(user_agent);
		auto host = FindHost(result);

		if(host != nullptr)
		{
			return host->id;
		}
	}

	// default: OME
	return P2P_OME_PEER_ID;
}

RtcP2PManager::PeerType RtcP2PManager::ParseUserAgent(ov::String user_agent)
{
	PeerType peer_type;

	if(user_agent.IsEmpty() == false)
	{
		// TODO: 여기서 UA를 기반으로 정보 추출
	}

	return peer_type;
}

std::shared_ptr<RtcP2PManager::HostInfo> RtcP2PManager::FindHost(PeerType client_type)
{
	for(auto &host : _available_list)
	{
		if(CheckCompatible(host.second->type, client_type))
		{
			return host.second;
		}
	}

	return nullptr;
}

bool RtcP2PManager::CheckCompatible(const RtcP2PManager::PeerType &host, const RtcP2PManager::PeerType &client)
{
	if(host.browser_type == BrowserType::Other)
	{
		return false;
	}

	return (host.browser_type == client.browser_type);
}
