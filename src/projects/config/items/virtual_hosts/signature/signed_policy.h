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
	struct SignedPolicy : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetPolicyQueryKey, _policy_query_key)
		CFG_DECLARE_REF_GETTER_OF(GetSignatureQueryKey, _signature_query_key)
		CFG_DECLARE_REF_GETTER_OF(GetSecretKey, _secret_key)

	protected:
		void MakeParseList() override
		{
			RegisterValue("PolicyQueryKey", &_policy_query_key);
			RegisterValue("SignatureQueryKey", &_signature_query_key);
			RegisterValue("SecretKey", &_secret_key);
		}

		ov::String _policy_query_key;
		ov::String _signature_query_key;
		ov::String _secret_key;
	};
}  // namespace cfg