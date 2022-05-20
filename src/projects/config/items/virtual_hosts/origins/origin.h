//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "pass.h"

namespace cfg
{
	namespace vhost
	{
		namespace orgn
		{
			struct Origin : public Item
			{
				CFG_DECLARE_CONST_REF_GETTER_OF(GetLocation, _location)
				CFG_DECLARE_CONST_REF_GETTER_OF(GetPass, _pass)
				CFG_DECLARE_CONST_REF_GETTER_OF(IsPersist, _persist)
				CFG_DECLARE_CONST_REF_GETTER_OF(IsFailBack, _fail_back)

			protected:
				void MakeList() override
				{
					Register("Location", &_location);
					Register("Pass", &_pass);
					Register<Optional>("Persist", &_persist);
					Register<Optional>("Failback", &_fail_back);
				}

				ov::String _location;
				Pass _pass;
				bool _persist = false;
				bool _fail_back = false;		
			};
		}  // namespace orgn
	}	   // namespace vhost
}  // namespace cfg