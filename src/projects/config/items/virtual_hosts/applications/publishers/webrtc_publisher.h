//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct WebrtcPublisher : public Publisher
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::Webrtc)

					CFG_DECLARE_GETTER_OF(GetTimeout, _timeout)
					CFG_DECLARE_GETTER_OF(IsRtxEnabled, _rtx)
					CFG_DECLARE_GETTER_OF(IsUlpfecEnalbed, _ulpfec)

				protected:
					void MakeParseList() override
					{
						Publisher::MakeParseList();

						RegisterValue<Optional>("Timeout", &_timeout);
						RegisterValue<Optional>("Rtx", &_rtx);
						RegisterValue<Optional>("Ulpfec", &_ulpfec);
					}

					int _timeout = 30000;
					bool _rtx = true;
					bool _ulpfec = true;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg