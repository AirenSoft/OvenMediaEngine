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

namespace cfg
{
	struct Providers : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetProviderList, _provider_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("RTMP", &_rtmp_provider, nullptr, [this]() -> bool {
				_provider_list.push_back(&_rtmp_provider);
				return true;
			});

			RegisterValue<Optional>("RTSP", &_rtsp_provider, nullptr, [this]() -> bool {
				_provider_list.push_back(&_rtsp_provider);
				return true;
			});
		};

		std::vector<const Provider *> _provider_list;

		RtmpProvider _rtmp_provider;
		RtspProvider _rtsp_provider;
	};
}  // namespace cfg