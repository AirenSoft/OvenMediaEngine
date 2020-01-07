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
	struct Provider : public Item
	{
		virtual ProviderType GetType() const = 0;
		CFG_DECLARE_GETTER_OF(GetMaxConnection, _max_connection)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("MaxConnection", &_max_connection);
		}

		int _max_connection = 0;
	};
}  // namespace cfg