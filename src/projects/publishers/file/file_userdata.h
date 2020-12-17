#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "base/info/record.h"


class FileUserdataSets 
{
public:
	FileUserdataSets();
	~FileUserdataSets();

	bool Set(ov::String userdata_id, std::shared_ptr<info::Record> userdata);
	
	uint32_t GetCount();

	std::shared_ptr<info::Record> GetByKey(ov::String key);
	std::shared_ptr<info::Record> GetBySessionId(session_id_t session_id);
	std::map<ov::String, std::shared_ptr<info::Record>>& GetUserdataSets();

	void DeleteByKey(ov::String key);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<info::Record>> _userdata_sets;
};