#include "file.h"
#include <dirent.h>

namespace ov
{
	std::tuple<bool, std::vector<ov::String>> File::GetFileList(ov::String directory_path)
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
}