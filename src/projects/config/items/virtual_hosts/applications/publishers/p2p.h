//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct P2P : public Item
	{
		CFG_DECLARE_GETTER_OF(GetClientPeersPerHostPeer, _client_peers_per_host_peer)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("ClientPeersPerHostPeer", &_client_peers_per_host_peer);
		}

		int _client_peers_per_host_peer = 2;
	};
}  // namespace cfg