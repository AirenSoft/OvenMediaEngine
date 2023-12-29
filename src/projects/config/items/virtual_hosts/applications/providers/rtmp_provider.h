//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"
#include "event_generator/event_generator.h"
#include "options/use_incoming_ts.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct RtmpProvider : public Provider, public UseIncomingTimestamp
				{
				protected:
					// true: block(disconnect) new incoming stream
					// false: don't block new incoming stream
					bool _is_block_duplicate_stream_name = true;
					EventGenerator _event_generator;
					bool _is_passthrough_output_profile = false;
				public:
					ProviderType GetType() const override
					{
						return ProviderType::Rtmp;
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(IsBlockDuplicateStreamName, _is_block_duplicate_stream_name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetEventGenerator, _event_generator)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsPassthroughOutputProfile, _is_passthrough_output_profile)
					
				protected:
					void MakeList() override
					{
						Provider::MakeList();

						Register<Optional>("BlockDuplicateStreamName", &_is_block_duplicate_stream_name);
						Register<Optional>("EventGenerator", &_event_generator);
						Register<Optional>("PassthroughOutputProfile", &_is_passthrough_output_profile);
						Register<Optional>("UseIncomingTimestamp", &_use_incoming_timestamp);
					}
				};
			}  // namespace pvd
		} // namespace app
	} // namespace vhost
}  // namespace cfg