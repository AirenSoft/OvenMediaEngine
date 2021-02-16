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
		namespace pub
		{
			struct IceCandidates : public Item
			{
			protected:
				std::vector<ov::String> _ice_candidate_list{"*:10000-10005/udp"};
				ov::String _tcp_relay;

			public:
				CFG_DECLARE_REF_GETTER_OF(GetIceCandidateList, _ice_candidate_list);
				CFG_DECLARE_REF_GETTER_OF(GetTcpRelay, _tcp_relay);

			protected:
				void MakeList() override
				{
					Register<Optional>("IceCandidate", &_ice_candidate_list);
					Register<Optional>("TcpRelay", &_tcp_relay);
				}
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg