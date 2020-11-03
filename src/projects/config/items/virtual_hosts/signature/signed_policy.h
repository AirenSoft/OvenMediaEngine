//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "enables.h"
#include "base/common_types.h"

namespace cfg
{
	struct SignedPolicy : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetPolicyQueryKey, _policy_query_key)
		CFG_DECLARE_REF_GETTER_OF(GetSignatureQueryKey, _signature_query_key)
		CFG_DECLARE_REF_GETTER_OF(GetSecretKey, _secret_key)
		CFG_DECLARE_REF_GETTER_OF(GetEnabledProviders, _enables.GetProviders().GetValue())
		CFG_DECLARE_REF_GETTER_OF(GetEnabledPublishers, _enables.GetPublishers().GetValue())

		bool IsEnabledProvider(ProviderType type) const 
		{
			auto type_str = StringFromProviderType(type);
			return _enables.GetProviders().IsExist(type_str);
		}

		bool IsEnabledPublisher(PublisherType type) const
		{
			auto type_str = StringFromPublisherType(type);
			return _enables.GetPublishers().IsExist(type_str);
		}

	protected:
		void MakeParseList() override
		{
			RegisterValue("PolicyQueryKey", &_policy_query_key);
			RegisterValue("SignatureQueryKey", &_signature_query_key);
			RegisterValue("SecretKey", &_secret_key);
			RegisterValue("Enables", &_enables);
		}

		ov::String _policy_query_key;
		ov::String _signature_query_key;
		ov::String _secret_key;

		Enables	_enables;
	};
}  // namespace cfg