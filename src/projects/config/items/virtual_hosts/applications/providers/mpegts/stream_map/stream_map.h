//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./stream.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				namespace mpegts
				{
					struct StreamMap : public Item
					{
						CFG_DECLARE_REF_GETTER_OF(GetStreamList, _stream_list)

					protected:
						void MakeParseList() override
						{
							RegisterValue("Stream", &_stream_list);
						}

						std::vector<Stream> _stream_list;
					};
				}  // namespace mpegts
			}	   // namespace pvd
		}		   // namespace app
	}			   // namespace vhost
}  // namespace cfg