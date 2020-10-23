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

bool RtmpPushUserdataSets::Set(ov::String userdata_id, std::shared_ptr<RtmpPushUserdata> userdata)
{
	_userdata_sets[userdata_id] = userdata;

	return true;
}

std::shared_ptr<RtmpPushUserdata> RtmpPushUserdataSets::GetAt(uint32_t index)
{
	auto iter( _userdata_sets.begin() );
    std::advance( iter, index );

    if(iter == _userdata_sets.end())
    {
    	return nullptr;
    }

	return iter->second;
}

ov::String RtmpPushUserdataSets::GetKeyAt(uint32_t index)
{
	auto iter( _userdata_sets.begin() );
    std::advance( iter, index );

    if(iter == _userdata_sets.end())
    {
    	return nullptr;
    }

	return iter->first;	
}

std::shared_ptr<RtmpPushUserdata> RtmpPushUserdataSets::GetByKey(ov::String key)
{
	auto iter = _userdata_sets.find(key);
	if(iter == _userdata_sets.end())
	{
		return nullptr;
	}

	return iter->second;
}


std::shared_ptr<RtmpPushUserdata> RtmpPushUserdataSets::GetBySessionId(session_id_t session_id)
{
	for ( auto &item : _userdata_sets )
	{
		auto userdata = item.second;

		if(userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

uint32_t RtmpPushUserdataSets::GetCount()
{
	return _userdata_sets.size();	
}