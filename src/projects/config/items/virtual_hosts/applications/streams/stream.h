//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "profiles/stream_profiles.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace stream
			{
				struct Stream : public Item
				{
					CFG_DECLARE_REF_GETTER_OF(Name, _name)
					CFG_DECLARE_REF_GETTER_OF(GetOutputStreamName, _output_stream_name)
					CFG_DECLARE_REF_GETTER_OF(GetProfileList, _profiles.GetProfileList())

				protected:
					void MakeParseList() override
					{
						RegisterValue("Name", &_name);
						RegisterValue("OutputStreamName", &_output_stream_name);
						RegisterValue("Profiles", &_profiles);
					}

					ov::String _name;
					ov::String _output_stream_name;
					prf::Profiles _profiles;
				};
			}  // namespace stream
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg