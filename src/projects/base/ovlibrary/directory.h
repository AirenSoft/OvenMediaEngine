//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./string.h"
#include <iostream>
#include <sys/stat.h>
#include <errno.h>

namespace ov
{
	bool IsDirExist(const ov::String &path)
	{
		struct stat info;
		if (stat(path.CStr(), &info) != 0)
		{
			return false;
		}
		else if (info.st_mode & S_IFDIR)
		{
			return true;
		}

		return false;
	}

	bool CreateDirectories(const ov::String &path)
	{
		if (IsDirExist(path))
		{
			return true;
		}

		mode_t mode = 0755;
		int ret = mkdir(path.CStr(), mode);
		if (ret == 0)
		{
			return true;
		}

		switch (errno)
		{
			case EEXIST:
				// Directory already exists
				return true;
			case ENOENT:
				// Parent directory doesn't exist, try to create it
				{
					// Strip one level
					auto parent_path = path.Substring(0, path.IndexOfRev('/'));
					if (CreateDirectories(parent_path))
					{
						// Parent directory was successfully created, try it again
						if (mkdir(path.CStr(), mode) == 0 || errno == EEXIST)
						{
							return true;
						}
					}
				}
				break;
			default:
				break;
		}

		return true;
	}
}