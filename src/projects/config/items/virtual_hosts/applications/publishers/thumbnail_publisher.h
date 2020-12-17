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
				struct ThumbnailPublisher : public Publisher
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::Thumbnail)

					CFG_DECLARE_GETTER_OF(GetCrossDomainList, _cross_domains.GetUrls())
					CFG_DECLARE_GETTER_OF(GetCrossDomains, _cross_domains)

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
