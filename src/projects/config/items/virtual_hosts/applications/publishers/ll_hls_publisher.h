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
				struct LLHlsPublisher : public Publisher, public cmn::CrossDomainSupport
				{
				protected:
					
					int _segment_count = 3;
					int _chunk_duration_ms = 1000;
					int _segment_duration = 4;

				public:
					PublisherType GetType() const override
					{
						return PublisherType::LLHls;
					}

					// CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetChunkDurationMs, _chunk_duration_ms)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentCount, _segment_count)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("ChunkDuration", &_chunk_duration_ms);
						Register<Optional>("SegmentDuration", &_segment_duration);
						Register<Optional>("SegmentCount", &_segment_count);
						Register<Optional>("CrossDomains", &_cross_domains);
					}
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
