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
	namespace p2p
	{
		struct P2P : public Item
		{
			CFG_DECLARE_GETTER_OF(GetMaxClientPeersPerHostPeer, _max_client_peers_per_host_peer)

		protected:
			void MakeParseList() override
			{
				RegisterValue<Optional>("MaxClientPeersPerHostPeer", &_max_client_peers_per_host_peer);
			}

			int _max_client_peers_per_host_peer = 2;
		};
	}  // namespace p2p
}  // namespace cfg