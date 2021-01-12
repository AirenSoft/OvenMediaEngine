//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./publisher.h"
#include "./webrtc/ice_candidates.h"
#include "./webrtc/ice_servers.h"
#include "./webrtc/signalling.h"

namespace cfg
{
	namespace bind
	{
		namespace pub
		{
			struct Webrtc : public Item
			{
			protected:
				Signalling _signalling;
				IceCandidates _ice_candidates;
				IceServers _ice_servers;

			public:
				explicit Webrtc(const char *port)
					: _signalling(port)
				{
				}

				Webrtc(const char *port, const char *tls_port)
					: _signalling(port, tls_port)
				{
				}

				CFG_DECLARE_REF_GETTER_OF(GetSignalling, _signalling)
				CFG_DECLARE_REF_GETTER_OF(GetIceCandidates, _ice_candidates)
				CFG_DECLARE_REF_GETTER_OF(GetIceServers, _ice_servers)

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("Signalling", &_signalling);
					RegisterValue<Optional>("IceCandidates", &_ice_candidates);
					RegisterValue<Optional>("IceServers", &_ice_servers);
				};
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg