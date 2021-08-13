//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once
#include "ovlibrary.h"

namespace ov
{
	class File
	{
	public:
		static std::tuple<bool, std::vector<ov::String>> GetFileList(ov::String directory_path);
	};
}