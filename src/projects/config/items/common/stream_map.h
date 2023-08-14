//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream.h"

namespace cfg
{
	namespace cmn
	{
		struct StreamMap : public Item
		{
		protected:
			std::vector<Stream> _stream_list;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetStreamList, _stream_list)

		protected:
			void MakeList() override
			{
				Register<OmitJsonName>("Stream", &_stream_list);
			}
		};
	}  // namespace cmn
}  // namespace cfg
