//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "p2p.h"
#include "publisher.h"

namespace cfg
{
	struct WebrtcPublisher : public Publisher
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(PublisherType, GetType, PublisherType::Webrtc)

		CFG_DECLARE_REF_GETTER_OF(GetP2P, _p2p)

	protected:
		void MakeParseList() override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("Timeout", &_timeout);
			RegisterValue<Optional>("P2P", &_p2p);
		}

		int _timeout = 0;
		P2P _p2p;
	};
}  // namespace cfg