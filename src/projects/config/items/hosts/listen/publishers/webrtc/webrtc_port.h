//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_candidates.h"

namespace cfg
{
	struct WebrtcPort : public Item
	{
		CFG_DECLARE_GETTER_OF(GetIceCandidates, _ice_candidates)
		CFG_DECLARE_GETTER_OF(GetSignalling, _signalling)

		CFG_DECLARE_GETTER_OF(GetSignallingPort, _signalling.GetPort())

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Signalling", &_signalling);
			RegisterValue<Optional>("IceCandidates", &_ice_candidates);
		}

		Port _signalling{"3333/tcp"};
		IceCandidates _ice_candidates;
	};
}  // namespace cfg
