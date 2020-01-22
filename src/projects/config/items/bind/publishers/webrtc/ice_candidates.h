//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_candidate.h"

namespace cfg
{
	struct IceCandidates : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetIceCandidateList, _ice_candidate_list);

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("IceCandidate", &_ice_candidate_list);
		}

		std::vector<IceCandidate> _ice_candidate_list{
			IceCandidate("*:10000-10005/udp")};
	};
}  // namespace cfg