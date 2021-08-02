//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace an
	{
		struct Analytics : public Item
		{
		protected:
			ov::String _server = "https://ovenconsole.com";
			ov::String _user_key = "user_key";

		public:
			CFG_DECLARE_REF_GETTER_OF(GetServer, _server)
			CFG_DECLARE_REF_GETTER_OF(GetUserKey, _user_key)

		protected:
			void MakeList() override
			{
				Register<Optional>("Server", &_server);
				Register<Optional>("UserKey", &_user_key);
			}
		};
	}  // namespace p2p
}  // namespace cfg