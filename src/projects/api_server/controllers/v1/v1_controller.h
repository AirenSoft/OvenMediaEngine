//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../controller.h"

namespace api
{
	namespace v1
	{
		class V1Controller : public Controller<V1Controller>
		{
		public:
			void PrepareHandlers() override;
		};
	}  // namespace v1
}  // namespace api