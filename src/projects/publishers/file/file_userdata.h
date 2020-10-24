#pragma once

#include "base/ovlibrary/ovlibrary.h"

class FileUserdata
{
public:
	static std::shared_ptr<FileUserdata> Create(bool eanble, ov::String vhost, ov::String app, ov::String stream, std::vector<int32_t>& tracks)
	{
		auto userdata = std::make_shared<FileUserdata>();

		userdata->SetEnable(eanble);
		userdata->SetVhost(vhost);
		userdata->SetApplication(app);
		userdata->SetStream(stream);
		userdata->SetSelectTrack(tracks);
		userdata->SetRemove(false);
		userdata->SetSessionId(0);

		return userdata;
	}

	FileUserdata() {}
	~FileUserdata() {}

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

	void SetSelectTrack(std::vector<int32_t>& tracks)
	{
		_selected_tracks.clear();
		_selected_tracks.assign( tracks.begin(), tracks.end() );
	}

	std::vector<int32_t>& GetSelectTrack()
	{
		return _selected_tracks;
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
		// info.AppendFormat("url=%s ", GetTargetUrl().CStr());
		// info.AppendFormat("stream_key=%s / ", GetTargetStreamKey().CStr());
		info.AppendFormat("session_id=%d ", GetSessionId());

		return info;
	}

private:

	bool _enable;

	// Source stream
	ov::String _vhost_name;
	ov::String _aplication_name;
	ov::String _stream_name;

	std::vector<int32_t> _selected_tracks;

	bool _remove;

	// Session Id
	session_id_t _session_id;
};


class FileUserdataSets 
{
public:
	FileUserdataSets();
	~FileUserdataSets();

	bool Set(ov::String userdata_id, std::shared_ptr<FileUserdata> userdata);
	
	uint32_t GetCount();

	ov::String GetKeyAt(uint32_t index);
	std::shared_ptr<FileUserdata> GetAt(uint32_t index);
	std::shared_ptr<FileUserdata> GetByKey(ov::String key);
	std::shared_ptr<FileUserdata> GetBySessionId(session_id_t session_id);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<FileUserdata>> _userdata_sets;
};
