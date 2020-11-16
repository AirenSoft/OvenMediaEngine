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

namespace cfg
{
	namespace bind
	{
		namespace pvd
		{
			struct Providers : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt);
				CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
				CFG_DECLARE_REF_GETTER_OF(GetRtsp, _rtsp)
				CFG_DECLARE_REF_GETTER_OF(GetMpegts, _mpegts);

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("OVT", &_ovt);
					RegisterValue<Optional>("RTMP", &_rtmp);
					RegisterValue<Optional>("RTSP", &_rtsp);
					RegisterValue<Optional>("MPEGTS", &_mpegts);
				};

				Provider<cmn::SingularPort> _ovt{"9000/tcp"};
				Provider<cmn::SingularPort> _rtmp{"1935/tcp"};
				Provider<cmn::SingularPort> _rtsp{"554/tcp"};
				Provider<cmn::RangedPort> _mpegts{"4000/udp"};
			};
		}  // namespace pvd
	}	   // namespace bind
}  // namespace cfg