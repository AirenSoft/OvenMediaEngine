//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encodes/encodes.h"

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
					Encodes _encodes;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputStreamName, _output_stream_name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetEncodes, _encodes)

				protected:
					void MakeList() override
					{
						Register("Name", &_name);
						Register("OutputStreamName", &_output_stream_name);
						Register<Optional>("Encodes", &_encodes);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
