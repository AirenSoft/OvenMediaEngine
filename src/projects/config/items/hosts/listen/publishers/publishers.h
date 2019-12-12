//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "webrtc/webrtc_port.h"

namespace cfg
{
	struct ListenPublishers : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetRelay, _relay);
		CFG_DECLARE_GETTER_OF(GetRelayPort, _relay.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
		CFG_DECLARE_GETTER_OF(GetRtmpPort, _rtmp.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetHlsP, _hls)
		CFG_DECLARE_GETTER_OF(GetHlsPort, _hls.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetDash, _dash)
		CFG_DECLARE_GETTER_OF(GetDashPort, _dash.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetWebrtc, _webrtc)
		CFG_DECLARE_GETTER_OF(GetWebrtcPort, _webrtc.GetSignalling())

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Relay", &_relay);
			RegisterValue<Optional>("RTMP", &_rtmp);
			RegisterValue<Optional>("HLS", &_hls);
			RegisterValue<Optional>("DASH", &_dash);
			RegisterValue<Optional>("WebRTC", &_webrtc);
		};

		Port _relay{"9000/tcp"};
		Port _rtmp{"1935/tcp"};
		Port _hls{"80/tcp"};
		Port _dash{"80/tcp"};
		WebrtcPort _webrtc;
	};
}  // namespace cfg