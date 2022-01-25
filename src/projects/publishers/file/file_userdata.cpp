#include "file_userdata.h"

#include <base/info/stream.h>
#include <base/publisher/stream.h>

#include "file_private.h"

FileUserdataSets::FileUserdataSets()
{
}

FileUserdataSets::~FileUserdataSets()
{
	_sets.clear();
}

bool FileUserdataSets::Set(ov::String userdata_id, std::shared_ptr<info::Record> userdata)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	_sets[userdata_id] = userdata;

	return true;
}

std::shared_ptr<info::Record> FileUserdataSets::GetByKey(ov::String key)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	auto iter = _sets.find(key);
	if (iter == _sets.end())
	{
		return nullptr;
	}

	return iter->second;
}

std::shared_ptr<info::Record> FileUserdataSets::GetBySessionId(session_id_t session_id)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	for (auto& item : _sets)
	{
		auto userdata = item.second;

		if (userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

std::shared_ptr<info::Record> FileUserdataSets::GetAt(uint32_t index)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	auto iter = _sets.begin();

	std::advance(iter, index);

	if (iter == _sets.end())
		return nullptr;

	return iter->second;
}

void FileUserdataSets::DeleteByKey(ov::String key)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	_sets.erase(key);
}

uint32_t FileUserdataSets::GetCount()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	return _sets.size();
}
