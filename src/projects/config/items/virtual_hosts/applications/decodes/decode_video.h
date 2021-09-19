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
		namespace app
		{
			namespace dec
			{
				struct DecodeVideo : public Item
				{
				public:
					CFG_DECLARE_REF_GETTER_OF(IsHardwareAcceleration, _hw_acceleration);

				protected:
					void MakeList() override
					{
						Register("HardwareAcceleration", &_hw_acceleration);
					}

					bool _hw_acceleration;
				};
			}  // namespace dec
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg