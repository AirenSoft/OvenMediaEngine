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
					PublisherType GetType() const override
					{
						return PublisherType::Webrtc;
					}

					CFG_DECLARE_REF_GETTER_OF(GetTimeout, _timeout)
					CFG_DECLARE_REF_GETTER_OF(IsRtxEnabled, _rtx)
					CFG_DECLARE_REF_GETTER_OF(IsUlpfecEnalbed, _ulpfec)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("Timeout", &_timeout);
						Register<Optional>("Rtx", &_rtx);
						Register<Optional>("Ulpfec", &_ulpfec);
					}

					int _timeout = 30000;
					bool _rtx = true;
					bool _ulpfec = true;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg