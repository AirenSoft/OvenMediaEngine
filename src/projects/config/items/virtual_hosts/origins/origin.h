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
				CFG_DECLARE_CONST_REF_GETTER_OF(IsFailback, _failback)
				CFG_DECLARE_CONST_REF_GETTER_OF(IsStrictLocation, _strict_location)
				CFG_DECLARE_CONST_REF_GETTER_OF(IsRelay, _relay)

			protected:
				void MakeList() override
				{
					Register("Location", &_location);
					Register("Pass", &_pass);
					Register<Optional>("Persist", &_persist);
					Register<Optional>("Failback", &_failback);
					Register<Optional>("StrictLocation", &_strict_location);
					Register<Optional>("Relay", &_relay);
				}
				ov::String _location;
				Pass _pass;
				bool _persist = false;
				bool _failback = false;
				bool _strict_location = false;
				bool _relay = true;
			};
		}  // namespace orgn
	}	   // namespace vhost
}  // namespace cfg