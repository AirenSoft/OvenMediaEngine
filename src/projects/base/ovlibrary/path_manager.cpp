//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <utility>

#include <errno.h>
#if defined(__APPLE__)
#include <sys/stat.h>
#include <mach-o/dyld.h>
typedef mode_t __mode_t;
#else
#include <linux/limits.h>
#endif
#include <unistd.h>
#include <wordexp.h>

#include "path_manager.h"

#include "memory_utilities.h"

namespace ov
{
	String PathManager::GetAppPath(String sub_path)
	{
#if defined(__APPLE__)
		uint32_t path_length = 0;

		if (_NSGetExecutablePath(nullptr, &path_length) == - 1 && path_length != 0)
		{
			auto buffer = std::make_unique<char[]>(path_length);
			if (_NSGetExecutablePath(buffer.get(), &path_length) != -1)
			{
				return Combine(Combine(ExtractPath(String(buffer.get(), path_length - 1)), std::move(sub_path)), "");
			}
		}
#elif !defined(__CYGWIN__)
		char buffer[4096] = {0};
		
		if (readlink("/proc/self/exe", buffer, OV_COUNTOF(buffer)) != -1)
		{
			return Combine(Combine(ExtractPath(buffer), std::move(sub_path)), "");
		}
#endif

		return "";
	}

	String PathManager::GetCurrentPath(String sub_path)
	{
		char buffer[4096] = {0};

		if (getcwd(buffer, OV_COUNTOF(buffer)) == nullptr)
		{
			return "";
		}

		return Combine(Combine(buffer, std::move(sub_path)), "");
	}

	String PathManager::ExpandPath(String path)
	{
		wordexp_t exp_result;

		if (wordexp(path, &exp_result, 0) == 0)
		{
			path = exp_result.we_wordv[0];
		}

		wordfree(&exp_result);

		return path;
	}

	String PathManager::ExtractPath(String path)
	{
		off_t position = path.IndexOfRev('/');

		if (position >= 0)
		{
			path = path.Left(static_cast<size_t>(position + 1));
		}

		return path;
	}

	bool PathManager::MakeDirectory(const char *path, int mask)
	{
		if ((path == nullptr) || (path[0] == '\0'))
		{
			return false;
		}

		return (mkdir(path, static_cast<mode_t>(mask)) == 0) || (errno == EEXIST);
	}

	String PathManager::Combine(String path1, String path2)
	{
		if ((path1.HasSuffix("/") == false) && (path2.HasPrefix("/") == false))
		{
			path1.Append("/");
		}

		path1.Append(path2);

		return path1;
	}

	bool PathManager::IsAbsolute(const char *path)
	{
		return (path != nullptr) && (path[0] == '/');
	}

	String PathManager::GetCanonicalPath(const char *path)
	{
		char buffer[PATH_MAX];

		auto result = realpath(path, buffer);

		if (result == nullptr)
		{
			return "";
		}

		return String(buffer);
	}
}  // namespace ov