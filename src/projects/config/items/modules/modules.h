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
#include "ll_hls.h"
#include "p2p.h"

namespace cfg
{
	namespace modules
	{
		struct modules : public Item
		{
		protected:
			HTTP2 _http2;
			LLHls _ll_hls;
			P2P _p2p;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetHttp2, _http2)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetLLHls, _ll_hls)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetP2P, _p2p)

		protected:
			void MakeList() override
			{
				Register<Optional>("HTTP2", &_http2);
				Register<Optional>("LLHLS", &_ll_hls);
				Register<Optional>({"P2P", "p2p"}, &_p2p);
			}
		};
	}  // namespace bind
}  // namespace cfg