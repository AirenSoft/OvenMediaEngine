//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rules/rules.h"
#include "webhooks.h"

namespace cfg
{
	namespace alrt
	{
		struct Alert : public Item
		{
		protected:
			ov::String _url;
			ov::String _secret_key;
			int _timeout_msec = 3000;

			Webhooks _webhooks;

			ov::String _rules_file;
			rule::Rules _rules;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetUrl, _url)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetSecretKey, _secret_key)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetTimeoutMsec, _timeout_msec)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetWebhooks, _webhooks)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetRulesFile, _rules_file)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetRules, _rules)

		protected:
			void MakeList() override
			{
				// <Webhooks> is the standard way to configure notification destinations.
				// It is Optional only because the deprecated <Url>/<SecretKey>/<Timeout>
				// below must still be accepted. At least one webhook must be configured,
				// which is validated in mon::alrt::Alert::Start().
				Register<Optional>("Webhooks", &_webhooks);

				// Deprecated. The legacy single webhook settings are kept for backward
				// compatibility during the deprecation period and will be removed in a
				// future release.
				Register<Optional>("Url", &_url, nullptr,
								   [=]() -> std::shared_ptr<ConfigError> {
									   logw("Config", "Alert.Url, Alert.SecretKey and Alert.Timeout are deprecated and will be removed in a future release. Please use Alert.Webhooks instead.");
									   return nullptr;
								   });
				Register<Optional>("SecretKey", &_secret_key);
				Register<Optional>("Timeout", &_timeout_msec, nullptr,
								   [=]() -> std::shared_ptr<ConfigError> {
									   if (_timeout_msec < 0)
									   {
										   return CreateConfigErrorPtr("Timeout must not be negative: %d", _timeout_msec);
									   }
									   return nullptr;
								   });
				Register<Optional, ResolvePath>("RulesFile", &_rules_file);
				Register<Optional>("Rules", &_rules);
			}
		};
	}  // namespace alrt
}  // namespace cfg
