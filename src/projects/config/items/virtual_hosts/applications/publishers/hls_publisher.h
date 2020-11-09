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
				struct HlsPublisher : public Publisher
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::Hls)

					CFG_DECLARE_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_GETTER_OF(GetCrossDomainList, _cross_domains.GetUrls())
					CFG_DECLARE_GETTER_OF(GetCrossDomains, _cross_domains)

				protected:
					void MakeParseList() override
					{
						Publisher::MakeParseList();

						RegisterValue<Optional>("SegmentCount", &_segment_count);
						RegisterValue<Optional>("SegmentDuration", &_segment_duration);
						RegisterValue<Optional>("CrossDomains", &_cross_domains);
					}

					int _segment_count = 3;
					int _segment_duration = 5;
					cmn::CrossDomains _cross_domains;
					int _send_buffer_size = 1024 * 1024 * 20;  // 20M
					int _recv_buffer_size = 0;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
