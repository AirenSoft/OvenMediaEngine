//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2026 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace alrt
	{
		struct Webhook : public Item
		{
		protected:
			ov::String _url;
			ov::String _secret_key;
			int _timeout_msec = 3000;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetUrl, _url)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetSecretKey, _secret_key)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetTimeoutMsec, _timeout_msec)

		protected:
			void MakeList() override
			{
				Register("Url", &_url);
				Register<Optional>("SecretKey", &_secret_key);
				Register<Optional>("Timeout", &_timeout_msec, nullptr,
								   [=]() -> std::shared_ptr<ConfigError> {
									   if (_timeout_msec < 0)
									   {
										   return CreateConfigErrorPtr("Timeout must not be negative: %d", _timeout_msec);
									   }
									   return nullptr;
								   });
			}
		};
	}  // namespace alrt
}  // namespace cfg
