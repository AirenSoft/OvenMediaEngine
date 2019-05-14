//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_candidates.h"

namespace cfg
{
	struct WebrtcPort : public Item
	{
		WebrtcPort(int signalling_port)
			: _signalling_port(signalling_port)
		{
		}

		const IceCandidates &GetIceCandidates() const
		{
			return _ice_candidates;
		}

		int GetSignallingPort() const
		{
			return _signalling_port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("IceCandidates", &_ice_candidates);
			RegisterValue("Signalling", &_signalling_port);
		}

		IceCandidates _ice_candidates;
		// Signalling port
		int _signalling_port = 3333;
	};
}
