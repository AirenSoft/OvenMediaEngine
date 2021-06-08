//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace sig
		{
			// SignedToken is for special purposes only. This is a private feature.
			struct SignedToken : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetCryptoKey, _crypto_key)
				CFG_DECLARE_REF_GETTER_OF(GetQueryStringKey, _query_string_key)

			protected:
				void MakeList() override
				{
					Register("CryptoKey", &_crypto_key);
					Register("QueryStringKey", &_query_string_key);
				}

				ov::String _crypto_key;
				ov::String _query_string_key;
			};
		}  // namespace sig
	}	   // namespace vhost
}  // namespace cfg