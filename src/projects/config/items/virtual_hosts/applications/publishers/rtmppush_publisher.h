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
				struct RtmpPushPublisher : public Publisher
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::RtmpPush)

					CFG_DECLARE_REF_GETTER_OF(GetCrossDomains, _cross_domain.GetUrls())

				protected:
					void MakeParseList() override
					{
						Publisher::MakeParseList();

						RegisterValue<Optional>("CrossDomain", &_cross_domain);
					}

					cmn::CrossDomain _cross_domain;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg