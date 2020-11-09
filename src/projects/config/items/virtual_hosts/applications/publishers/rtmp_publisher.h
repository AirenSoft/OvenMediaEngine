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
				struct RtmpPublisher : public Publisher
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::Rtmp)

					CFG_DECLARE_REF_GETTER_OF(GetCrossDomains, _cross_domains.GetUrls())

				protected:
					void MakeParseList() override
					{
						Publisher::MakeParseList();

						RegisterValue<Optional>("CrossDomains", &_cross_domains);
					}

					cmn::CrossDomains _cross_domains;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg