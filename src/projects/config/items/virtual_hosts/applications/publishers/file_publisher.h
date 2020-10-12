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
	struct FilePublisher : public Publisher
	{
		CFG_DECLARE_OVERRIDED_GETTER_OF(GetType, PublisherType::File)
		CFG_DECLARE_GETTER_OF(GetFilePath, _file_path)
		CFG_DECLARE_GETTER_OF(GetFileInfoPath, _file_info_path)

	protected:
		void MakeParseList() override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("FilePath", &_file_path);
			RegisterValue<Optional>("FileInfoPath", &_file_info_path);
		}

		ov::String _file_path = "";
		ov::String _file_info_path = "";
	};
}  // namespace cfg