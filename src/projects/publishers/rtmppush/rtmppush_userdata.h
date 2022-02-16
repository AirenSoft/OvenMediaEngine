#pragma once

#include "base/info/push.h"
#include "base/ovlibrary/ovlibrary.h"

class RtmpPushUserdataSets
{
public:
	RtmpPushUserdataSets();
	~RtmpPushUserdataSets();

	bool Set(std::shared_ptr<info::Push> userdata);

	uint32_t GetCount();

	std::shared_ptr<info::Push> GetAt(uint32_t index);
	std::shared_ptr<info::Push> GetByKey(ov::String key);
	std::shared_ptr<info::Push> GetBySessionId(session_id_t session_id);
	void DeleteByKey(ov::String key);

private:
	std::shared_mutex _mutex;
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<info::Push>> _sets;
};