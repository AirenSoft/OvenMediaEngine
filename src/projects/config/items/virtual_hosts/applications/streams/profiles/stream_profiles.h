//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream_profile.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace stream
			{
				namespace prf
				{
					struct Profiles : public Item
					{
						CFG_DECLARE_REF_GETTER_OF(GetProfileList, _profile_list)

					protected:
						void MakeParseList() override
						{
							RegisterValue("Profile", &_profile_list);
						}

						std::vector<Profile> _profile_list;
					};
				}  // namespace prf
			}	   // namespace stream
		}		   // namespace app
	}			   // namespace vhost
}  // namespace cfg