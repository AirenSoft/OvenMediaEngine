//=============================================================================
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
				struct LlDashPublisher : public Publisher, public cmn::CrossDomainSupport
				{
				protected:
					// LL-DASH uses time-based segment
					// int _segment_count = 3;
					int _segment_duration = 3;

					cmn::UtcTiming _utc_timing;

				public:
					PublisherType GetType() const override
					{
						return PublisherType::LlDash;
					}

					// CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentDuration, _segment_duration)

					CFG_DECLARE_CONST_REF_GETTER_OF(GetUtcTiming, _utc_timing)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						// Register<Optional>("SegmentCount", &_segment_count);
						Register<Optional>("SegmentDuration", &_segment_duration);

						Register<Optional>("UTCTiming", &_utc_timing);

						Register<Optional>("CrossDomains", &_cross_domains);
					}
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
