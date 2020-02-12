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
		std::vector<const Provider *> GetProviderList() const
		{
			return {
				&_rtmp_provider,
				&_rtsp_pull_provider,
				&_rtsp_provider};
		}

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("RTMP", &_rtmp_provider);
			RegisterValue<Optional>("RTSPPull", &_rtsp_pull_provider);
			RegisterValue<Optional>("RTSP", &_rtsp_provider);
		};

		RtmpProvider _rtmp_provider;
		RtspPullProvider _rtsp_pull_provider;
		RtspProvider _rtsp_provider;
	};
}  // namespace cfg