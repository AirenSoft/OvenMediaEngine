//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "module_template.h"

namespace cfg
{
	namespace modules
	{
		struct Recovery : public ModuleTemplate
		{
		protected:
			int _delete_lazy_stream_timeout = 0;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetDeleteLazyStreamTimeout, _delete_lazy_stream_timeout)

		protected:
			void MakeList() override
			{
				// Experimental feature is disabled by default
				SetEnable(false);
								
				ModuleTemplate::MakeList();
				Register<Optional>("DeleteLazyStreamTimeout", &_delete_lazy_stream_timeout);
			}
		};
	}  // namespace recovery
}  // namespace cfg