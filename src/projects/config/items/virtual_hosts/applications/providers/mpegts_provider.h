//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts/stream_map/stream_map.h"
#include "provider.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct MpegtsProvider : public Provider
				{
					CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, ProviderType::Mpegts)

					CFG_DECLARE_REF_GETTER_OF(GetStreamMap, _stream_map)

				protected:
					void MakeParseList() override
					{
						Provider::MakeParseList();

						RegisterValue<Optional>("StreamMap", &_stream_map);
					}

					mpegts::StreamMap _stream_map;
				};
			}  // namespace pvd
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg