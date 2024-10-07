//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "module_template.h"

namespace cfg
{
	namespace modules
	{
		struct DynamicAppRemoval : public ModuleTemplate
		{
		protected:

		public:

		protected:
			void MakeList() override
			{
				SetEnable(false);
				ModuleTemplate::MakeList();
			}
		};
	} // namespace modules
} // namespace cfg