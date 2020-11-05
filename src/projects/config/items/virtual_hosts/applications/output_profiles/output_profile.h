//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct OutputProfile : public Item
				{
				protected:
					ov::String _name;
					ov::String _output_stream_name;
					std::vector<enc::Encode> _encodes;

				public:
				protected:
					void MakeParseList() override
					{
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
