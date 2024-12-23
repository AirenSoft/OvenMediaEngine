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
		struct ETag : public ModuleTemplate
		{
		protected:

		public:

		protected:
			void MakeList() override
			{
				// Experimental feature is disabled by default
				SetEnable(false);
				
				ModuleTemplate::MakeList();
			}
		};
	} // namespace modules
} // namespace cfg