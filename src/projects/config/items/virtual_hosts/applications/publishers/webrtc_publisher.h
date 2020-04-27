//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"

namespace cfg
{
	struct WebrtcPublisher : public Publisher
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(PublisherType, GetType, PublisherType::Webrtc)

	protected:
		void MakeParseList() override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("Timeout", &_timeout);
		}

		int _timeout = 0;
	};
}  // namespace cfg