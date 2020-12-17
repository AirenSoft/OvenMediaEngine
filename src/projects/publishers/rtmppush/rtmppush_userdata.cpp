#include <base/info/stream.h>
#include <base/publisher/stream.h>
#include "rtmppush_userdata.h"
#include "rtmppush_private.h"


RtmpPushUserdataSets::RtmpPushUserdataSets()
{

}

RtmpPushUserdataSets::~RtmpPushUserdataSets()
{

}

bool RtmpPushUserdataSets::Set(ov::String userdata_id, std::shared_ptr<info::Push> userdata)
{
	_userdata_sets[userdata_id] = userdata;

	return true;
}

std::shared_ptr<info::Push> RtmpPushUserdataSets::GetByKey(ov::String key)
{
	auto iter = _userdata_sets.find(key);
	if(iter == _userdata_sets.end())
	{
		return nullptr;
	}

	return iter->second;
}

std::shared_ptr<info::Push> RtmpPushUserdataSets::GetBySessionId(session_id_t session_id)
{
	for ( auto &item : _userdata_sets )
	{
		auto userdata = item.second;

		if(userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

void RtmpPushUserdataSets::DeleteByKey(ov::String key)
{
	_userdata_sets.erase(key);
}


uint32_t RtmpPushUserdataSets::GetCount()
{
	return _userdata_sets.size();	
}

std::map<ov::String, std::shared_ptr<info::Push>>& RtmpPushUserdataSets::GetUserdataSets()
{
	return _userdata_sets;
}