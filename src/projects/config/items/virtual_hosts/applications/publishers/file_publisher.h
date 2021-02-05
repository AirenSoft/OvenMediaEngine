//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct FilePublisher : public Publisher
				{
					PublisherType GetType() const override
					{
						return PublisherType::File;
					}
					
					CFG_DECLARE_REF_GETTER_OF(GetFilePath, _file_path)
					CFG_DECLARE_REF_GETTER_OF(GetInfoPath, _info_path)
					CFG_DECLARE_REF_GETTER_OF(GetRootPath, _root_path)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("RootPath", &_root_path);
						Register<Optional>("FilePath", &_file_path);
						Register<Optional>("InfoPath", &_info_path);

						//@deprecated
						Register<Optional>("FileInfoPath", &_info_path);
					}

					ov::String _root_path = "";
					ov::String _file_path = "";
					ov::String _info_path = "";
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg