//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "string.h"

#include <sys/stat.h>

namespace ov
{
	class PathManager
	{
	public:
		static String GetAppPath(String sub_path = "");
		static String GetCurrentPath(String sub_path = "");

		static String ExpandPath(String path);
		static String ExtractPath(String path);

		// <app_path>/oss 디렉토리 준비
		// 만약, sub_path가 명시되어 있다면 <app_path>/oss/<sub_path> 경로까지 생성함
		static bool PrepareAppPath(String sub_path, String *created_path = NULL);

		// mask를 지정하지 않으면 755 (rwxr-xr-x)로 생성
		static bool MakeDirectory(const char *path, int mask = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

		// path1/path2 형태로 만듦
		static String Combine(String path1, String path2);

		static bool IsAbsolute(const char *path);
		static String GetCanonicalPath(const char *path);
	};
}