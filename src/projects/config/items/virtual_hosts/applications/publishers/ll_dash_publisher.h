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
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::LlDash)

					// CFG_DECLARE_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_GETTER_OF(GetCrossDomainList, _cross_domain.GetUrls())
					CFG_DECLARE_GETTER_OF(GetCrossDomain, _cross_domain)

				protected:
					void MakeParseList() override
					{
						Publisher::MakeParseList();

						// RegisterValue<Optional>("SegmentCount", &_segment_count);
						RegisterValue<Optional>("SegmentDuration", &_segment_duration);
						RegisterValue<Optional>("CrossDomain", &_cross_domain);
					}

					// LL-DASH uses time-based segment
					// int _segment_count = 3;
					int _segment_duration = 3;
					cmn::CrossDomain _cross_domain;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
