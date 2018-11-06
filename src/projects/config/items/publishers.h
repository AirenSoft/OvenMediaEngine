//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_publisher.h"
#include "hls_publisher.h"
#include "dash_publisher.h"
#include "webrtc_publisher.h"

namespace cfg
{
	struct Publishers : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("RTMP", &_rtmp_publisher);
			result = result && RegisterValue<Optional>("HLS", &_hls_publisher);
			result = result && RegisterValue<Optional>("DASH", &_dash_publisher);
			result = result && RegisterValue<Optional>("WebRTC", &_webrtc_publisher);

			return result;
		}

	protected:
		Value<RtmpPublisher> _rtmp_publisher;
		Value<HlsPublisher> _hls_publisher;
		Value<DashPublisher> _dash_publisher;
		Value<WebrtcPublisher> _webrtc_publisher;
	};
}