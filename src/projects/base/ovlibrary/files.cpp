//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./string.h"
#include <iostream>
#include <sys/stat.h>
#include <errno.h>
#include <ftw.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#include <linux/limits.h>

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

	static int RemoveFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb)
	{
		if(remove(pathname) < 0)
		{
			return -1;
		}

		return 0;
	}

	bool DeleteDirectories(const ov::String &path)
	{
		// Delete all files in the directory
		if (nftw(path.CStr(), RemoveFiles, 64, FTW_DEPTH | FTW_PHYS) < 0)
		{
			return false;
		}

		return true;
	}
	
	std::tuple<bool, std::vector<ov::String>> GetFileList(const ov::String &directory_path)
	{
		std::vector<ov::String> file_list;
		DIR *dir = opendir(directory_path.CStr());
		struct dirent *entry;

		if(dir == nullptr)
		{
			return {false, file_list};
		}

		while((entry = readdir(dir)) != nullptr)
		{
			auto file_path = ov::String::FormatString("%s/%s", directory_path.CStr(), entry->d_name);
			file_list.push_back(file_path);
		}

		closedir(dir);

		return {true, file_list};
	}

	ov::String GetBinaryPath()
	{
		char path[1024];
		ssize_t len = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
		if (len == -1)
		{
			return "";
		}
		
		return dirname(path);
	}
}