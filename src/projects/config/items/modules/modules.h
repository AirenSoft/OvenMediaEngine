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
#include "recovery.h"
#include "dynamic_app_removal.h"

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
			Recovery _recovery;
			DynamicAppRemoval _dynamic_app_removal;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetHttp2, _http2)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetLLHls, _ll_hls)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetP2P, _p2p)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetRecovery, _recovery)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetDynamicAppRemoval, _dynamic_app_removal)

		protected:
			void MakeList() override
			{
				Register<Optional>("HTTP2", &_http2);
				Register<Optional>("LLHLS", &_ll_hls);
				Register<Optional>({"P2P", "p2p"}, &_p2p);
				Register<Optional>("Recovery", &_recovery);
				Register<Optional>("DynamicAppRemoval", &_dynamic_app_removal);
			}
		};
	}  // namespace modules
}  // namespace cfg