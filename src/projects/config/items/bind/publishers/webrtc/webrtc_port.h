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
		explicit WebrtcPort(const char *port)
			: _signalling(port)
		{
		}

		explicit WebrtcPort(const char *port, const char *tls_port)
			: _signalling(port, tls_port)
		{
		}

		CFG_DECLARE_GETTER_OF(GetSignalling, _signalling)
		CFG_DECLARE_GETTER_OF(GetSignallingPort, _signalling.GetPort())
		CFG_DECLARE_GETTER_OF(GetSignallingTlsPort, _signalling.GetTlsPort())

		CFG_DECLARE_GETTER_OF(GetIceCandidates, _ice_candidates)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Signalling", &_signalling);
			RegisterValue<Optional>("IceCandidates", &_ice_candidates);
		}

		TlsPort _signalling;
		IceCandidates _ice_candidates;
	};
}  // namespace cfg
