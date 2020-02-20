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
	struct SignedUrl : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetCryptoKey, _crypto_key)
		CFG_DECLARE_REF_GETTER_OF(GetQueryStringKey, _query_string_key)

	protected:
		void MakeParseList() override
		{
			RegisterValue("CryptoKey", &_crypto_key);
			RegisterValue("QueryStringKey", &_query_string_key);
		}

		ov::String _crypto_key;
		ov::String _query_string_key;
	};
}  // namespace cfg