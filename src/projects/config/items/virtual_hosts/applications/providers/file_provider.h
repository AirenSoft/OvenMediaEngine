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

				protected:
					void MakeList() override
					{
						Provider::MakeList();

						Register<Optional>("RootPath", &_root_path);
					}

					ov::String _root_path = "";
				};
			}  // namespace pvd
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg