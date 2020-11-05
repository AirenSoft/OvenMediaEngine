//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./publisher.h"
#include "./webrtc.h"

namespace cfg
{
	namespace bind
	{
		namespace pub
		{
			struct Publishers : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt)
				CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
				CFG_DECLARE_REF_GETTER_OF(GetHls, _hls)
				CFG_DECLARE_REF_GETTER_OF(GetDash, _dash)
				CFG_DECLARE_REF_GETTER_OF(GetWebrtc, _webrtc)

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("OVT", &_ovt);
					RegisterValue<Optional>("RTMP", &_rtmp);
					RegisterValue<Optional>("HLS", &_hls);
					RegisterValue<Optional>("DASH", &_dash);
					RegisterValue<Optional>("WebRTC", &_webrtc);
				};

				Publisher<cmn::SingularPort> _ovt{"9000/tcp"};
				Publisher<cmn::SingularPort> _rtmp{"1935/tcp"};
				Publisher<cmn::SingularPort> _hls{"80/tcp", "443/tcp"};
				Publisher<cmn::SingularPort> _dash{"80/tcp", "443/tcp"};
				Webrtc _webrtc{"3333/tcp", "3334/tcp"};
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg