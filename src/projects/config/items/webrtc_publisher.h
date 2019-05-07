//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "ice_candidates.h"
#include "signalling.h"
#include "p2p.h"

namespace cfg
{
	struct WebrtcPublisher : public Publisher
	{
		WebrtcPublisher()
			: Publisher(0)
		{
		}

		const IceCandidates &GetIceCandidates() const
		{
			return _ice_candidates;
		}

		PublisherType GetType() const override
		{
			return PublisherType::Webrtc;
		}

		const Signalling &GetSignalling() const
		{
			return _signalling;
		}

		const P2P &GetP2P() const
		{
			return _p2p;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue("IceCandidates", &_ice_candidates);
			RegisterValue<Optional>("Timeout", &_timeout);
			RegisterValue("Signalling", &_signalling);
			RegisterValue<Optional>("P2P", &_p2p);
		}

		IceCandidates _ice_candidates;
		int _timeout = 0;
		Signalling _signalling;
		P2P _p2p;
	};
}