//=============================================================================
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
			namespace oprf
			{
				struct Decodes : public Item
				{
				public:
					// Informal Option 
					CFG_DECLARE_CONST_REF_GETTER_OF(IsKeyframeOnlyIfNeed, _keyframe_only_if_need);

				protected:
					void MakeList() override
					{
						Register<Optional>("KeyframeOnlyIfNeed", &_keyframe_only_if_need);
					}

					bool _keyframe_only_if_need = false;
				};
			}  // namespace dec
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg