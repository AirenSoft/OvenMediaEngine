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
#include "../common/webrtc/webrtc.h"

namespace cfg
{
	namespace bind
	{
		namespace pub
		{
			struct Publishers : public Item
			{
			protected:
				Publisher<cmn::SingularPort> _ovt{"9000/tcp"};
				Publisher<cmn::SingularPort> _rtmp{"1935/tcp"};
				Publisher<cmn::SingularPort> _hls{"80/tcp", "443/tcp"};
				Publisher<cmn::SingularPort> _dash{"80/tcp", "443/tcp"};
				Publisher<cmn::SingularPort> _thumbnail{"80/tcp", "443/tcp"};

				cmm::Webrtc _webrtc{"3333/tcp", "3334/tcp"};

			public:
				CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt)
				CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
				CFG_DECLARE_REF_GETTER_OF(GetHls, _hls)
				CFG_DECLARE_REF_GETTER_OF(GetDash, _dash)
				CFG_DECLARE_REF_GETTER_OF(GetWebrtc, _webrtc)
				CFG_DECLARE_REF_GETTER_OF(GetThumbnail, _thumbnail)

			protected:
				void MakeList() override
				{
					Register<Optional>({"OVT", "ovt"}, &_ovt);
					Register<Optional>({"RTMP", "rtmp"}, &_rtmp);
					Register<Optional>({"HLS", "hls"}, &_hls);
					Register<Optional>({"DASH", "dash"}, &_dash);
					Register<Optional>({"WebRTC", "webrtc"}, &_webrtc);
					Register<Optional>({"Thumbnail", "thumbnail"}, &_thumbnail);
				};
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg