//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct P2P : public Item
	{
		int GetClientPeersPerHostPeer() const
		{
			return _client_peers_per_host_peer;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("ClientPeersPerHostPeer", &_client_peers_per_host_peer);
		}

		int _client_peers_per_host_peer = 2;
	};
}