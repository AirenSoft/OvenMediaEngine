//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace orgn
		{
			struct Properties : public Item
			{
				CFG_DECLARE_CONST_REF_GETTER_OF(GetNoInputFailoverTimeout, _no_input_failover_timeout)
				CFG_DECLARE_CONST_REF_GETTER_OF(GetUnusedStreamDeletionTimeout, _unused_stream_deletion_timeout)

			protected:
				void MakeList() override
				{
					Register<Optional>("NoInputFailoverTimeout", &_no_input_failover_timeout);
					Register<Optional>("UnusedStreamDeletionTimeout", &_unused_stream_deletion_timeout);
				}

				int64_t _no_input_failover_timeout = 3000;
				int64_t _unused_stream_deletion_timeout = 60000;
			};
		}  // namespace orgn
	}	   // namespace vhost
}  // namespace cfg