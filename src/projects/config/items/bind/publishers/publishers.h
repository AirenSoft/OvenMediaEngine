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
	struct BindPublishers : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt)
		CFG_DECLARE_GETTER_OF(GetOvtPort, _ovt.GetPort())

		CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
		CFG_DECLARE_GETTER_OF(GetRtmpPort, _rtmp.GetPort())

		CFG_DECLARE_REF_GETTER_OF(GetHls, _hls)
		CFG_DECLARE_GETTER_OF(GetHlsPort, _hls.GetPort())
		CFG_DECLARE_GETTER_OF(GetHlsTlsPort, _hls.GetTlsPort())

		CFG_DECLARE_REF_GETTER_OF(GetDash, _dash)
		CFG_DECLARE_GETTER_OF(GetDashPort, _dash.GetPort())
		CFG_DECLARE_GETTER_OF(GetDashTlsPort, _dash.GetTlsPort())

		CFG_DECLARE_REF_GETTER_OF(GetWebrtc, _webrtc)
		CFG_DECLARE_GETTER_OF(GetWebrtcPort, _webrtc.GetSignallingPort())
		CFG_DECLARE_GETTER_OF(GetWebrtcTlsPort, _webrtc.GetSignallingTlsPort())

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("OVT", &_ovt);
			RegisterValue<Optional>("RTMP", &_rtmp);
			RegisterValue<Optional>("HLS", &_hls);
			RegisterValue<Optional>("DASH", &_dash);
			RegisterValue<Optional>("WebRTC", &_webrtc);
		};

		Port _ovt{"9000/tcp"};
		Port _rtmp{"1935/tcp"};
		TlsPort _hls{"80/tcp", "443/tcp"};
		TlsPort _dash{"80/tcp", "443/tcp"};
		WebrtcPort _webrtc{"3333/tcp", "3334/tcp"};
	};
}  // namespace cfg