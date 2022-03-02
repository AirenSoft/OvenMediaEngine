//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <sys/stat.h>

#include "./error.h"
#include "./string.h"

namespace ov
{
	class PathManager
	{
	public:
		static String GetAppPath(String sub_path = "");
		static String GetCurrentPath(String sub_path = "");

		static String ExpandPath(String path);
		// /var/log/test
		// ~~~~~~~~        <== Extract this part
		static String ExtractPath(String path);
		// /var/log/test
		//          ~~~~   <== Extract this part
		static String ExtractFileName(String path);

		// Creates a directory with the mask (Default mask is 755 (rwxr-xr-x))
		static bool MakeDirectory(const char *path, int mask = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

		// Creates a directory named "<path1>/<path2."
		static String Combine(String path1, String path2);

		static bool IsFile(String path);
		static bool IsDirectory(String path);

		static bool IsAbsolute(const char *path);
		static String GetCanonicalPath(const char *path);

		static String ExtractExtension(String path);

		static std::shared_ptr<ov::Error> GetFileList(const ov::String &base_file_name, const ov::String &pattern, std::vector<ov::String> *file_list, bool exclude_base_path = true);

		static std::shared_ptr<ov::Error> Rename(const ov::String &file_name, const ov::String &to_file_name);
		static std::shared_ptr<ov::Error> DeleteFile(const ov::String &file_name);
	};
}  // namespace ov
