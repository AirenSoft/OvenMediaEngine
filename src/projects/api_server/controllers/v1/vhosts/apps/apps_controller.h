//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../controller.h"

namespace api
{
	namespace v1
	{
		// /v1/vhosts
		class AppsController : public Controller<AppsController>
		{
		public:
			void PrepareHandlers() override;

		protected:
			ApiResponse OnGetAppList(const std::shared_ptr<HttpClient> &client);
			ApiResponse OnGetApp(const std::shared_ptr<HttpClient> &client);
		};
	}  // namespace v1
}  // namespace api
