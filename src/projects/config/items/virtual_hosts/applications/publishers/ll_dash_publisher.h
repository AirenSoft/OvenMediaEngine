//=============================================================================
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
				struct LlDashPublisher : public Publisher
				{
					PublisherType GetType() const override
					{
						return PublisherType::LlDash;
					}

					// CFG_DECLARE_REF_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_REF_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_REF_GETTER_OF(GetCrossDomainList, _cross_domains.GetUrls())
					CFG_DECLARE_REF_GETTER_OF(GetCrossDomains, _cross_domains)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						// Register<Optional>("SegmentCount", &_segment_count);
						Register<Optional>("SegmentDuration", &_segment_duration);
						Register<Optional>("CrossDomains", &_cross_domains);
					}

					// LL-DASH uses time-based segment
					// int _segment_count = 3;
					int _segment_duration = 3;
					cmn::CrossDomains _cross_domains;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
