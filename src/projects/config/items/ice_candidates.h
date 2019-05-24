//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_candidate.h"

namespace cfg
{
	struct IceCandidates : public Item
	{
		const std::vector<IceCandidate> &GetIceCandidateList() const
		{
			return _ice_candidate_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("IceCandidate", &_ice_candidate_list);
		}

		std::vector<IceCandidate> _ice_candidate_list {
			IceCandidate("*:10000-10005/udp")
		};
	};
}