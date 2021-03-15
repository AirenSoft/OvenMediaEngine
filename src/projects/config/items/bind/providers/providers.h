//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./provider.h"
#include "../common/webrtc/webrtc.h"

namespace cfg
{
	namespace bind
	{
		namespace pvd
		{
			struct Providers : public Item
			{
			protected:
				Provider<cmn::SingularPort> _ovt{"9000/tcp"};
				Provider<cmn::SingularPort> _rtmp{"1935/tcp"};
				Provider<cmn::SingularPort> _rtsp{"554/tcp"};
				Provider<cmn::RangedPort> _mpegts{"4000/udp"};

				cmm::Webrtc _webrtc{"3333/tcp", "3334/tcp"};

			public:
				CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt);
				CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
				CFG_DECLARE_REF_GETTER_OF(GetRtsp, _rtsp)
				CFG_DECLARE_REF_GETTER_OF(GetMpegts, _mpegts);

				CFG_DECLARE_REF_GETTER_OF(GetWebrtc, _webrtc)

			protected:
				void MakeList() override
				{
					Register<Optional>({"OVT", "ovt"}, &_ovt);
					Register<Optional>({"RTMP", "rtmp"}, &_rtmp);
					Register<Optional>({"RTSP", "rtsp"}, &_rtsp);
					Register<Optional>({"MPEGTS", "mpegts"}, &_mpegts);
					Register<Optional>({"WebRTC", "webrtc"}, &_webrtc);
				};
			};
		}  // namespace pvd
	}	   // namespace bind
}  // namespace cfg