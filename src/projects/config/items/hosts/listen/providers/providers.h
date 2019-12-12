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
	struct ListenProviders : public Item
	{
		CFG_DECLARE_GETTER_OF(GetRtmpPort, _rtmp.GetPort())

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("RTMP", &_rtmp);
		};

		Port _rtmp{"1935/tcp"};
	};
}  // namespace cfg