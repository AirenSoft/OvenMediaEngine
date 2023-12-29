//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
                struct UseIncomingTimestamp
                {
                protected:
                    bool _use_incoming_timestamp = false;

                public:
                    const auto &IsIncomingTimestampUsed() const
                    {
                        return _use_incoming_timestamp;
                    }
                };
			} // namespace pvd
		} // namespace app
	} // namespace vhost
} // namespace cfg