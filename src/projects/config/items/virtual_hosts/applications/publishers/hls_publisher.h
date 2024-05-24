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
#include "hls_options/dvr.h"
#include "hls_options/default_query_string.h"
#include "publisher.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct HlsPublisher : public Publisher, public cmn::CrossDomainSupport
				{
				protected:
					int _segment_count = 3;
					int _segment_duration = 5;
					Dvr _dvr;
					DefaultQueryString _default_query_string;

				public:
					PublisherType GetType() const override
					{
						return PublisherType::Hls;
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentCount, _segment_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSegmentDuration, _segment_duration)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDvr, _dvr)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDefaultQueryString, _default_query_string)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("SegmentCount", &_segment_count);
						Register<Optional>("SegmentDuration", &_segment_duration);
						Register<Optional>("CrossDomains", &_cross_domains);
						Register<Optional>("DVR", &_dvr);
						Register<Optional>("DefaultQueryString", &_default_query_string);
					}
				};
			}  // namespace pub
		} // namespace app
	} // namespace vhost
}  // namespace cfg
