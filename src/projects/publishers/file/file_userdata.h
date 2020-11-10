#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "base/info/record.h"
/*
class FileUserdata
{
public:
	static std::shared_ptr<FileUserdata> Create(ov::String id, ov::String vhost, ov::String app, const std::shared_ptr<info::Record> &record)
	{
		auto userdata = std::make_shared<FileUserdata>();

		userdata->SetId(id);
		userdata->SetVhost(vhost);
		userdata->SetApplication(app);
		userdata->SetEnable(false);
		userdata->SetRemove(false);
		userdata->SetSessionId(0);

		userdata->SetRecord(record);

		return userdata;
	}

	FileUserdata() {}
	~FileUserdata() {}

	void SetId(ov::String value)
	{
		_id = value;
	}

	ov::String GetId() 
	{
		return _id;
	}

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


	ov::String GetStreamName()
	{
		return _record->GetStream().GetName();
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

	void SetRecord(const std::shared_ptr<info::Record> &record)
	{
		_record = record;
	}

	std::shared_ptr<info::Record>& GetRecord()
	{
		return _record;
	}

	ov::String GetInfoString() {
		ov::String info = "";

		info.AppendFormat("_id=%s\n", GetId().CStr());
		info.AppendFormat("_remove=%s\n", GetRemove()?"true":"false");
		info.AppendFormat("_enable=%s\n", GetEnable()?"true":"false");
		info.AppendFormat("_vhost=%s\n", GetVhost().CStr());
		info.AppendFormat("_app=%s\b", GetApplication().CStr());
		info.AppendFormat("_stream=%s\b", GetStreamName().CStr());
		info.AppendFormat("_session_id=%d", GetSessionId());

		return info;
	}

private:
	// User Defined Id
	ov::String 	_id;

	// Enabled/Disabled Flag
	bool _enable;

	// Remove Flag
	bool _remove;

	// Virtual Host
	ov::String _vhost_name;

	// Application
	ov::String _aplication_name;

	// Record Info
	std::shared_ptr<info::Record> _record;

	// File Session Id
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
	
	void DeleteByKey(ov::String key);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<FileUserdata>> _userdata_sets;
};
*/

class FileUserdataSets 
{
public:
	FileUserdataSets();
	~FileUserdataSets();

	bool Set(ov::String userdata_id, std::shared_ptr<info::Record> userdata);
	
	uint32_t GetCount();

	ov::String GetKeyAt(uint32_t index);
	std::shared_ptr<info::Record> GetAt(uint32_t index);
	std::shared_ptr<info::Record> GetByKey(ov::String key);
	std::shared_ptr<info::Record> GetBySessionId(session_id_t session_id);
	
	void DeleteByKey(ov::String key);

private:
	// <userdata_id, userdata>
	std::map<ov::String, std::shared_ptr<info::Record>> _userdata_sets;
};