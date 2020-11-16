//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace mgr
	{
		struct API : public Item
		{
		protected:
			ov::String _access_token;

		public:
			CFG_DECLARE_REF_GETTER_OF(GetAccessToken, _access_token)

		protected:
			void MakeParseList() override
			{
				RegisterValue("AccessToken", &_access_token);
			}
		};
	}  // namespace mgr
}  // namespace cfg