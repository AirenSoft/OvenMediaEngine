#pragma once

#include "base/ovlibrary/ovlibrary.h"

class RtmpPushUserdata
{
public:
	static std::shared_ptr<RtmpPushUserdata> Create(bool eanble, ov::String vhost, ov::String app, ov::String stream, ov::String target_url, ov::String target_stream_key)
	{
		auto userdata = std::make_shared<RtmpPushUserdata>();

		userdata->SetEnable(eanble);
		userdata->SetVhost(vhost);
		userdata->SetApplication(app);
		userdata->SetStream(stream);
		userdata->SetTargetUrl(target_url);
		userdata->SetTargetStreamKey(target_stream_key);
		userdata->SetRemove(false);
		userdata->SetSessionId(0);

		return userdata;
	}

	RtmpPushUserdata() {}
	~RtmpPushUserdata() {}

	void SetEnable(bool eanble) 
	{ 
		_enable = eanble; 
	}
	bool GetEnable() 
	{ 
		return _enable; 
	}

	void SetVhost(ov::String value)
	{
		_vhost_name = value;
	}
	ov::String GetVhost()
	{
		return _vhost_name;
	}

	void SetApplication(ov::String value)
	{
		_aplication_name = value;
	}
	ov::String GetApplication()
	{
		return _aplication_name;
	}

	void SetStream(ov::String value)
	{
		_stream_name = value;
	}
	ov::String GetStream()
	{
		return _stream_name;
	}

	void SetTargetUrl(ov::String value)
	{
		_target_url = value;
	}
	ov::String GetTargetUrl()
	{
		return _target_url;
	}

	void SetTargetStreamKey(ov::String value)
	{
		_target_stream_key = value;
	}
	ov::String GetTargetStreamKey()
	{
		return _target_stream_key;
	}

	void SetRemove(bool value)
	{
		_remove = value;
	}

	bool GetRemove()
	{
		return _remove;
	}

	void SetSessionId(session_id_t id)
	{
		_session_id = id;
	}

	session_id_t GetSessionId() {
		return _session_id;
	}

	ov::String GetInfoString() {
		ov::String info = "";

		info.AppendFormat("remove=%s ", GetRemove()?"true":"false");
		info.AppendFormat("enable=%s ", GetEnable()?"true":"false");
		info.AppendFormat("vhost=%s ", GetVhost().CStr());
		info.AppendFormat("app=%s ", GetApplication().CStr());
		info.AppendFormat("stream=%s / ", GetStream().CStr());
		info.AppendFormat("url=%s ", GetTargetUrl().CStr());
		info.AppendFormat("stream_key=%s / ", GetTargetStreamKey().CStr());
		info.AppendFormat("session_id=%d ", GetSessionId());

		return info;
	}


private:
	bool _enable;
	ov::String _vhost_name;
	ov::String _aplication_name;
	ov::String _stream_name;

	ov::String _target_url;
	ov::String _target_stream_key;

	bool _remove;

	session_id_t _session_id;
};

class RtmpPushUserdataSets 
{
public:
	RtmpPushUserdataSets();
	~RtmpPushUserdataSets();

	bool Set(ov::String userdata_id, std::shared_ptr<RtmpPushUserdata> userdata);
	
	uint32_t GetCount();

	ov::String GetKeyAt(uint32_t index);
	std::shared_ptr<RtmpPushUserdata> GetAt(uint32_t index);
	std::shared_ptr<RtmpPushUserdata> GetByKey(ov::String key);
	std::shared_ptr<RtmpPushUserdata> GetBySessionId(session_id_t session_id);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<RtmpPushUserdata>> _userdata_sets;
};
