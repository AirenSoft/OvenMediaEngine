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
	RtcPeerBrowser browser;

	if(user_agent.IsEmpty())
	{
		return nullptr;
	}

	// TODO(dimiden): Extract OS/Browser information from UA
	browser.browser_type = RtcBrowserType::Chrome;

	auto peer_info = std::make_shared<RtcPeerInfo>((PrivateToken){});

	peer_info->_id = id;
	peer_info->_browser = browser;
	peer_info->_response = response;

	peer_info->_can_accept =
		!((browser.browser_type == RtcBrowserType::Edge) ||
		  (browser.browser_type == RtcBrowserType::Other));

	return peer_info;
}

bool RtcPeerInfo::IsCompatibleWith(const std::shared_ptr<RtcPeerInfo> &peer)
{
	return true;

	//if(host.browser_type == BrowserType::Other)
	//{
	//	return false;
	//}
	//
	//return (host.browser_type == client.browser_type);
}
