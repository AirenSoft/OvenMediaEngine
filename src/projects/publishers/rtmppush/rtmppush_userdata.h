#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "base/info/push.h"



class RtmpPushUserdataSets 
{
public:
	RtmpPushUserdataSets();
	~RtmpPushUserdataSets();

	bool Set(ov::String userdata_id, std::shared_ptr<info::Push> userdata);
	
	uint32_t GetCount();

	ov::String GetKeyAt(uint32_t index);
	std::shared_ptr<info::Push> GetAt(uint32_t index);
	std::shared_ptr<info::Push> GetByKey(ov::String key);
	std::shared_ptr<info::Push> GetBySessionId(session_id_t session_id);
	
	void DeleteByKey(ov::String key);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<info::Push>> _userdata_sets;
};