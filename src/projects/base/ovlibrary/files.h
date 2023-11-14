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
#include <ftw.h>

namespace ov
{
	bool IsDirExist(const ov::String &path);
	bool CreateDirectories(const ov::String &path);
	static int RemoveFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
	bool DeleteDirectories(const ov::String &path);
	std::tuple<bool, std::vector<ov::String>> GetFileList(const ov::String &directory_path);
	ov::String GetBinaryPath();

	// If path is relative path, return absolute path from binary path.
	// If path is absolute path, return path.
	ov::String GetAbsolutePath(const ov::String &path);
}