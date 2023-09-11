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
#include "dumps/dumps.h"
#include "ll_hls_cache_control.h"
#include "ll_hls_dvr.h"
#include "ll_hls_drm.h"
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
					
					bool _origin_mode = false;
					int _segment_count = 10;
					double _chunk_duration = 0.5;
					double _part_hold_back = 0; // it will be set to 3 * chunk_duration automatically
					int _segment_duration = 6;
					Dumps _dumps;
					LLHlsCacheControl _cache_control;
					LLHlsDvr _dvr;
					LLHlsDrm _drm;
					bool _server_time_based_segment_numbering = false;

				public:
					PublisherType GetType() const override
					{
						return PublisherType::LLHls;
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(IsOriginMode, _origin_mode)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsServerTimeBasedSegmentNumbering, _server_time_based_segment_numbering)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetChunkDuration, _chunk_duration)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetPartHoldBack, _part_hold_back)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDumps, _dumps)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetCacheControl, _cache_control)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDvr, _dvr)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDrm, _drm)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("OriginMode", &_origin_mode);
						Register<Optional>("ServerTimeBasedSegmentNumbering", &_server_time_based_segment_numbering);
						Register<Optional>("ChunkDuration", &_chunk_duration);
						Register<Optional>("PartHoldBack", &_part_hold_back);
						Register<Optional>("SegmentDuration", &_segment_duration);
						Register<Optional>("SegmentCount", &_segment_count);
						Register<Optional>("CrossDomains", &_cross_domains);
						Register<Optional>("Dumps", &_dumps);
						Register<Optional>("CacheControl", &_cache_control);
						Register<Optional>("DVR", &_dvr);
						Register<Optional>("DRM", &_drm);
					}
				};
			}  // namespace pub
		} // namespace app
	} // namespace vhost
}  // namespace cfg
