//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "port.h"
#include "webrtc_port.h"

namespace cfg
{
	struct Ports : public Item
	{
		const Port &GetOriginPort() const
		{
			return _origin_port;
		}

		const Port &GetRtmpProviderPort() const
		{
			return _rtmp_provider_port;
		}

		const Port &GetRtmpPort() const
		{
			return _rtmp_port;
		}

		const Port &GetHlsPort() const
		{
			return _hls_port;
		}

		const Port &GetDashPort() const
		{
			return _dash_port;
		}

		const WebrtcPort &GetWebrtcPort() const
		{
			return _webrtc_port;
		}

	protected:
		void MakeParseList() const override
		{
			// Origin
			RegisterValue<Optional>("Origin", &_origin_port);

			// Providers
			RegisterValue<Optional>("RTMPProvider", &_rtmp_provider_port);

			// Publishers
			RegisterValue<Optional>("RTMP", &_rtmp_port);
			RegisterValue<Optional>("HLS", &_hls_port);
			RegisterValue<Optional>("DASH", &_dash_port);
			RegisterValue<Optional>("WebRTC", &_webrtc_port);
		}

		// Listen port for Origin
		Port _origin_port { 9000 };

		// Listen port for Providers
		Port _rtmp_provider_port { 1935 };

		// Listen port for Publishers
		Port _rtmp_port { 1935 };
		Port _hls_port { 80 };
		Port _dash_port { 80 };
		WebrtcPort _webrtc_port { 3333 };
	};
}