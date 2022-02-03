//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../common/cross_domain_support.h"
#include "publisher.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct MpegtsPushPublisher : public Publisher, public cmn::CrossDomainSupport
				{
				public:
					PublisherType GetType() const override
					{
						return PublisherType::MpegtsPush;
					}

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>({"CrossDomains", OmitRule::Omit}, &_cross_domains);
					}
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg