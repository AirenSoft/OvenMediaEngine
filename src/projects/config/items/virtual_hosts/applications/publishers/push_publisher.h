//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../common/cross_domain_support.h"
#include "publisher.h"
#include "stream_map/stream_map.h"
namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct PushPublisher : public Publisher
				{
				public:
					PublisherType GetType() const override
					{
						return PublisherType::Push;
					}

				protected:
					pub::StreamMap _stream_map;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetStreamMap, _stream_map)

				protected:
					void MakeList() override
					{
						Register<Optional>({"StreamMap", "streamMap"}, &_stream_map);
					}
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg