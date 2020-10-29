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
	struct LlDashPublisher : public Publisher
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::LlDash)

		// CFG_DECLARE_GETTER_OF(GetSegmentCount, _segment_count)
		CFG_DECLARE_GETTER_OF(GetSegmentDuration, _segment_duration)
		CFG_DECLARE_GETTER_OF(GetCrossDomains, _cross_domain.GetUrls())

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
		int _segment_duration = 5;
		CrossDomain _cross_domain;
	};
}  // namespace cfg
