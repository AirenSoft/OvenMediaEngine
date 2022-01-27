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
				struct PlayoutDelay : public Item
				{
				protected:
					int _min = 0;
					int _max = 0;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetMin, _min)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetMax, _max)

				protected:
					void MakeList() override
					{
						Register("Min", &_min);
						Register("Max", &_max);
					}
				};

				struct WebrtcPublisher : public Publisher
				{
					PublisherType GetType() const override
					{
						return PublisherType::Webrtc;
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetTimeout, _timeout)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsRtxEnabled, _rtx)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsUlpfecEnalbed, _ulpfec)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsJitterBufferEnabled, _jitter_buffer)

					CFG_DECLARE_CONST_REF_GETTER_OF(GetPlayoutDelay, _playout_delay)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("Timeout", &_timeout);
						Register<Optional>("JitterBuffer", &_jitter_buffer);
						Register<Optional>("Rtx", &_rtx);
						Register<Optional>("Ulpfec", &_ulpfec);
						Register<Optional>("PlayoutDelay", &_playout_delay);
					}

					int _timeout = 30000;
					bool _rtx = false;
					bool _ulpfec = false;
					bool _jitter_buffer = false;
					PlayoutDelay _playout_delay;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg