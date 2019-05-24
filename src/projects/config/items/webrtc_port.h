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
#include "port.h"

namespace cfg
{
	struct WebrtcPort : public Port
	{
		explicit WebrtcPort(const char *port)
			: Port(port)
		{
		}

		// Alias of GetPort()
		int GetSignallingPort() const
		{
			return GetPort();
		}

		const IceCandidates &GetIceCandidates() const
		{
			return _ice_candidates;
		}

	protected:
		using Port::GetPort;

		void MakeParseList() const override
		{
			// Port::MakeParseList();

			RegisterValue<Optional>("IceCandidates", &_ice_candidates);
			RegisterValue<Optional>("Signalling", &_port);
		}

		IceCandidates _ice_candidates;
	};
}
