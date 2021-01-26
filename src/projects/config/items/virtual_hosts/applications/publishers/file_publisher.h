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
					CFG_DECLARE_REF_GETTER_OF(GetFileInfoPath, _file_info_path)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("FilePath", &_file_path);
						Register<Optional>("FileInfoPath", &_file_info_path);
					}

					ov::String _file_path = "";
					ov::String _file_info_path = "";
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg