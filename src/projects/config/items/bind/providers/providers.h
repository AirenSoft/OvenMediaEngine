//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct BindProviders : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetOvt, _ovt);
		CFG_DECLARE_GETTER_OF(GetOvtPort, _ovt.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetRtmp, _rtmp)
		CFG_DECLARE_GETTER_OF(GetRtmpPort, _rtmp.GetPort())
		CFG_DECLARE_REF_GETTER_OF(GetRtsp, _rtsp)
		CFG_DECLARE_GETTER_OF(GetRtspPort, _rtsp.GetPort())

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("OVT", &_ovt);
			RegisterValue<Optional>("RTMP", &_rtmp);
			RegisterValue<Optional>("RTSP", &_rtsp);
		};

		Port _ovt{"9000/tcp"};
		Port _rtmp{"1935/tcp"};
		Port _rtsp{"554/tcp"};
	};
}  // namespace cfg