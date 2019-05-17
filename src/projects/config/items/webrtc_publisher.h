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
#include "p2p.h"

namespace cfg
{
	struct WebrtcPublisher : public Publisher
	{
		PublisherType GetType() const override
		{
			return PublisherType::Webrtc;
		}

		const P2P &GetP2P() const
		{
			return _p2p;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("Timeout", &_timeout);
			RegisterValue<Optional>("P2P", &_p2p);
		}

		int _timeout = 0;
		P2P _p2p;
	};
}