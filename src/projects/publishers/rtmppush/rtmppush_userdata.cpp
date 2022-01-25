#include "rtmppush_userdata.h"

#include <base/info/stream.h>
#include <base/publisher/stream.h>

#include "rtmppush_private.h"

RtmpPushUserdataSets::RtmpPushUserdataSets()
{
}

RtmpPushUserdataSets::~RtmpPushUserdataSets()
{
	_sets.clear();
}

bool RtmpPushUserdataSets::Set(std::shared_ptr<info::Push> userdata)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	_sets[userdata->GetId()] = userdata;

	return true;
}

std::shared_ptr<info::Push> RtmpPushUserdataSets::GetByKey(ov::String key)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	auto iter = _sets.find(key);
	if (iter == _sets.end())
	{
		return nullptr;
	}

	return iter->second;
}

std::shared_ptr<info::Push> RtmpPushUserdataSets::GetBySessionId(session_id_t session_id)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);
	for (auto &item : _sets)
	{
		auto userdata = item.second;

		if (userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

std::shared_ptr<info::Push> RtmpPushUserdataSets::GetAt(uint32_t index)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	auto iter = _sets.begin();

	std::advance(iter, index);

	if (iter == _sets.end())
		return nullptr;

	return iter->second;
}

void RtmpPushUserdataSets::DeleteByKey(ov::String key)
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	_sets.erase(key);
}

uint32_t RtmpPushUserdataSets::GetCount()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	return _sets.size();
}
