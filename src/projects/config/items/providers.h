//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_provider.h"
#include "rtsp_provider.h"

namespace cfg
{
	struct Providers : public Item
	{
		std::vector<const Provider *> GetProviderList() const
		{
			return {
				&_rtmp_provider,
				&_rtsp_provider
			};
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("RTMP", &_rtmp_provider);
			RegisterValue<Optional>("RTSP", &_rtsp_provider);
		};

		std::vector<const Provider *> _providers;

		RtmpProvider _rtmp_provider;
		RtspProvider _rtsp_provider;
	};
}
