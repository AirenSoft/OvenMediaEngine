//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtc_peer_info.h"

std::shared_ptr<RtcPeerInfo> RtcPeerInfo::FromUserAgent(peer_id_t id, const ov::String &user_agent, const std::shared_ptr<WebSocketClient> &response)
{
	if(user_agent.IsEmpty())
	{
		return nullptr;
	}

	RtcPeerBrowser browser = ParseBrowserInfo(user_agent);

	auto peer_info = std::make_shared<RtcPeerInfo>((PrivateToken){});

	peer_info->_id = id;
	peer_info->_browser = browser;
	peer_info->_response = response;

	peer_info->_can_accept =
		!((browser.browser_type == RtcBrowserType::Edge) ||
		  (browser.browser_type == RtcBrowserType::Other));

	return peer_info;
}

RtcPeerBrowser RtcPeerInfo::ParseBrowserInfo(const ov::String &user_agent)
{
	RtcPeerBrowser browser;

	if(user_agent.IndexOf("Chrome") >= 0)
	{
		browser.browser_type = RtcBrowserType::Chrome;
	}
	else if(user_agent.IndexOf("Safari") >= 0)
	{
		browser.browser_type = RtcBrowserType::Safari;
	}
	else if(user_agent.IndexOf("FireFox") >= 0)
	{
		browser.browser_type = RtcBrowserType::FireFox;
	}
	else if(user_agent.IndexOf("Edge") >= 0)
	{
		browser.browser_type = RtcBrowserType::Edge;
	}

	return std::move(browser);
}

bool RtcPeerInfo::IsCompatibleWith(const std::shared_ptr<RtcPeerInfo> &peer)
{
	if(_browser.browser_type == RtcBrowserType::Other)
	{
		// Unknown browser cannot become host peer
		return false;
	}

	return (_browser.browser_type == peer->_browser.browser_type);
}
