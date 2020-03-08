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
		CFG_DECLARE_OVERRIDED_GETTER_OF(PublisherType, GetType, PublisherType::LlDash)

		CFG_DECLARE_GETTER_OF(GetSegmentCount, _segment_count)
		CFG_DECLARE_GETTER_OF(GetSegmentDuration, _segment_duration)
		CFG_DECLARE_GETTER_OF(GetCrossDomains, _cross_domain.GetUrls())
		CFG_DECLARE_GETTER_OF(GetThreadCount, _thread_count > 0 ? _thread_count : 1)

	protected:
		void MakeParseList() override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("SegmentCount", &_segment_count);
			RegisterValue<Optional>("SegmentDuration", &_segment_duration);
			RegisterValue<Optional>("CrossDomain", &_cross_domain);
			RegisterValue<Optional>("ThreadCount", &_thread_count);
		}

		int _segment_count = 3;
		int _segment_duration = 5;
		CrossDomain _cross_domain;
		int _thread_count = 4;
	};
}  // namespace cfg
