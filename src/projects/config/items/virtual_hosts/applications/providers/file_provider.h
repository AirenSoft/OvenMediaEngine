//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"
#include "./file/stream_map.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct FileProvider : public Provider
				{
					ProviderType GetType() const override
					{
						return ProviderType::File;
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetRootPath, _root_path)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetStreamMap, _stream_map)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsPassthroughOutputProfile, _is_passthrough_output_profile)

				protected:
					void MakeList() override
					{
						Provider::MakeList();

						Register<Optional>("RootPath", &_root_path);
						Register<Optional>("StreamMap", &_stream_map);
						Register<Optional>("PassthroughOutputProfile", &_is_passthrough_output_profile);
					}

					ov::String _root_path = "";
					file::StreamMap _stream_map;
					bool _is_passthrough_output_profile = false;
				};
			}  // namespace pvd
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg