//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http2.h"
#include "p2p.h"

namespace cfg
{
	namespace modules
	{
		struct modules : public Item
		{
		protected:
			HTTP2 _http2;
			P2P _p2p;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetHttp2, _http2)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetP2P, _p2p)

		protected:
			void MakeList() override
			{
				Register<Optional>("HTTP2", &_http2);
				Register<Optional>({"P2P", "p2p"}, &_p2p);
			}
		};
	}  // namespace bind
}  // namespace cfg