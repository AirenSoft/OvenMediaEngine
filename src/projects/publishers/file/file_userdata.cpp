#include <base/info/stream.h>
#include <base/publisher/stream.h>
#include "file_userdata.h"
#include "file_private.h"

FileUserdataSets::FileUserdataSets()
{

}

FileUserdataSets::~FileUserdataSets()
{

}

bool FileUserdataSets::Set(ov::String userdata_id, std::shared_ptr<info::Record> userdata)
{
	_userdata_sets[userdata_id] = userdata;

	return true;
}


std::shared_ptr<info::Record> FileUserdataSets::GetByKey(ov::String key)
{
	auto iter = _userdata_sets.find(key);
	if(iter == _userdata_sets.end())
	{
		return nullptr;
	}

	return iter->second;
}

std::shared_ptr<info::Record> FileUserdataSets::GetBySessionId(session_id_t session_id)
{
	for ( auto &item : _userdata_sets )
	{
		auto userdata = item.second;

		if(userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

void FileUserdataSets::DeleteByKey(ov::String key)
{
	_userdata_sets.erase(key);
}


uint32_t FileUserdataSets::GetCount()
{
	return _userdata_sets.size();	
}

std::map<ov::String, std::shared_ptr<info::Record>>& FileUserdataSets::GetUserdataSets()
{
	return _userdata_sets;
}