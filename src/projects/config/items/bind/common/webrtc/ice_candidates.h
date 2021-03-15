//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace bind
	{
		namespace cmm
		{
			struct IceCandidates : public Item
			{
			protected:
				std::vector<ov::String> _ice_candidate_list{"*:10000-10005/udp"};
				ov::String _tcp_relay;

				int _tcp_relay_worker_count{};
				int _ice_worker_count{};

			public:
				CFG_DECLARE_REF_GETTER_OF(GetIceCandidateList, _ice_candidate_list);
				CFG_DECLARE_REF_GETTER_OF(GetTcpRelay, _tcp_relay);

				CFG_DECLARE_REF_GETTER_OF(GetTcpRelayWorkerCount, _tcp_relay_worker_count);
				CFG_DECLARE_REF_GETTER_OF(GetIceWorkerCount, _ice_worker_count);

			protected:
				void MakeList() override
				{
					Register<Optional>("IceCandidate", &_ice_candidate_list);
					Register<Optional>("TcpRelay", &_tcp_relay);

					Register<Optional>("TcpRelayWorkerCount", &_tcp_relay_worker_count);
					Register<Optional>("IceWorkerCount", &_ice_worker_count);
				}
			};
		}  // namespace cmm
	}	   // namespace bind
}  // namespace cfg