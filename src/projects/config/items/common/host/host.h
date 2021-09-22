//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "names.h"
#include "tls.h"

namespace cfg
{
	namespace cmn
	{
		struct Host : public Item
		{
		protected:
			Names _names;
			Tls _tls;

		public:
			CFG_DECLARE_REF_GETTER_OF(GetNameList, _names.GetNameList())
			CFG_DECLARE_REF_GETTER_OF(GetNames, _names)
			CFG_DECLARE_REF_GETTER_OF(GetTls, _tls)

		protected:
			void MakeList() override
			{
				Register({"Names", OmitRule::Omit}, &_names);
				Register<Optional>({"TLS", "tls"}, &_tls);
			}
		};
	}  // namespace cmn
}  // namespace cfg