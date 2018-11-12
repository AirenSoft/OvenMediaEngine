//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"

namespace cfg
{
	struct RtmpProvider : public Provider
	{
		ProviderType GetType() const override
		{
			return ProviderType::Rtmp;
		}

		int GetPort() const
		{
			return _port;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();

			RegisterValue<Optional>("Port", &_port);
		}

		int _port = 1935;
	};
}