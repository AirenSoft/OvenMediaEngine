//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "storage/storage.h"

namespace cfg
{
	namespace mgr
	{
		namespace api
		{
			struct API : public Item
			{
			protected:
				ov::String _access_token;

				Storage _storage;

			public:
				CFG_DECLARE_REF_GETTER_OF(GetAccessToken, _access_token)

				CFG_DECLARE_REF_GETTER_OF(GetStorage, _storage)

			protected:
				void MakeList() override
				{
					Register("AccessToken", &_access_token);

					Register<Optional>("Storage", &_storage);
				}
			};
		}  // namespace api
	}	   // namespace mgr
}  // namespace cfg