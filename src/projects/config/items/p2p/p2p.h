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
		protected:
			int _max_client_peers_per_host_peer = 2;

		public:
			CFG_DECLARE_REF_GETTER_OF(GetMaxClientPeersPerHostPeer, _max_client_peers_per_host_peer)

		protected:
			void MakeList() override
			{
				Register<Optional>("MaxClientPeersPerHostPeer", &_max_client_peers_per_host_peer);
			}
		};
	}  // namespace p2p
}  // namespace cfg