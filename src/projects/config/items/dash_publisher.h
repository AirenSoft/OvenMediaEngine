//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "tls.h"

namespace cfg
{
	struct DashPublisher : public Publisher
	{
		PublisherType GetType() const override
		{
			return PublisherType::Dash;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("Port", &_port);
			RegisterValue<Optional>("TLS", &_tls);
		}

		int _port = 80;
		Tls _tls;
	};
}