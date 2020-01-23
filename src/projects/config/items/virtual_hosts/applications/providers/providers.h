//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_provider.h"
#include "rtsp_provider.h"
#include "rtsp_pull_provider.h"

namespace cfg
{
	struct Providers : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetProviderList, _provider_list)

	protected:
		void MakeParseList() override
		{
			_provider_list.push_back(&_rtmp_provider);
			_provider_list.push_back(&_rtsp_pull_provider);
			_provider_list.push_back(&_rtsp_provider);

			RegisterValue<Optional>("RTMP", &_rtmp_provider);
			RegisterValue<Optional>("RTSPPull", &_rtsp_pull_provider);
			RegisterValue<Optional>("RTSP", &_rtsp_provider);
		};

		std::vector<const Provider *> _provider_list;

		RtmpProvider _rtmp_provider;
		RtspPullProvider _rtsp_pull_provider;
		RtspProvider _rtsp_provider;
	};
}  // namespace cfg