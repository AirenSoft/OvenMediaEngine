//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "module_template.h"

namespace cfg
{
	namespace modules
	{
		struct LLHls : public ModuleTemplate
		{
		protected:

		public:

		protected:
			void MakeList() override
			{
				ModuleTemplate::MakeList();
			}
		};
	} // namespace modules
} // namespace cfg