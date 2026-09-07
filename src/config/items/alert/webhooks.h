//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2026 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "webhook.h"

namespace cfg
{
	namespace alrt
	{
		struct Webhooks : public Item
		{
		protected:
			std::vector<Webhook> _webhook_list;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetWebhookList, _webhook_list)

		protected:
			void MakeList() override
			{
				Register("Webhook", &_webhook_list);
			}
		};
	}  // namespace alrt
}  // namespace cfg
