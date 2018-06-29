#include <unistd.h>
#include <errno.h>

#include "path_manager.h"

#include "memory_utilities.h"

namespace ov
{
	String PathManager::GetAppPath(String sub_path)
	{
		char buffer[4096] = { 0 };

#ifndef __CYGWIN__
		if(readlink("/proc/self/exe", buffer, OV_COUNTOF(buffer)) != -1)
		{
			String path = buffer;

			int nPosition = path.IndexOfRev('/');

			if(nPosition >= 0)
			{
				path = path.Left(nPosition + 1);
			}

			return Combine(Combine(path, sub_path), "");
		}
#endif

		return "";
	}

	bool PathManager::MakeDirectory(String path, int mask)
	{
		if(path.GetLength() == 0)
		{
			return false;
		}

		return (mkdir(path.CStr(), mask) == 0) || (errno == EEXIST);
	}

	String PathManager::Combine(String path1, String path2)
	{
		if(path1.Right(1) != "/")
		{
			path1.Append("/");
		}

		path1.Append(path2);

		return path1;
	}
}