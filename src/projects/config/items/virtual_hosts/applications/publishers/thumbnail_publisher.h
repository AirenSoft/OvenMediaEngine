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
				struct ThumbnailPublisher : public Publisher, public cmn::CrossDomainSupport
				{
				protected:
					cmn::CrossDomains _cross_domains;

				public:
					PublisherType GetType() const override
					{
						return PublisherType::Thumbnail;
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
