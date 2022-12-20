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
}