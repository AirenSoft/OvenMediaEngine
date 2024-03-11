//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "file_record.h"

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
					
					CFG_DECLARE_CONST_REF_GETTER_OF(GetFilePath, _file_path)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetInfoPath, _info_path)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetRootPath, _root_path)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetRecord, 	_record)

				protected:
					void MakeList() override
					{
						Publisher::MakeList();

						Register<Optional>("RootPath", &_root_path);
						Register<Optional>("FilePath", &_file_path);
						Register<Optional>("InfoPath", &_info_path);

						Register<Optional>("Record", &_record);

						//@deprecated

					}

					ov::String _root_path = "";
					ov::String _file_path = "";
					ov::String _info_path = "";
					
					FileRecord _record;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg