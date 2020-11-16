//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace cmn
	{
		struct Url : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetUrl, _url)

		protected:
			void MakeParseList() override
			{
				RegisterValue<ValueType::Text>(nullptr, &_url);
			}

			ov::String _url;
		};
	}  // namespace cmn
}  // namespace cfg