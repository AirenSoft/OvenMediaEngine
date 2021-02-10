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
					PublisherType GetType() const override
					{
						return PublisherType::RtmpPush;
					}

					CFG_DECLARE_REF_GETTER_OF(GetCrossDomains, _cross_domains.GetUrls())

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("CrossDomains", &_cross_domains);
					}

					cmn::CrossDomains _cross_domains;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg