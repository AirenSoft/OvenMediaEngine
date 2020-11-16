#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "push.h"
#include "stream.h"

namespace info
{
	Push::Push()
	{
		_created_time = std::chrono::system_clock::now();
		_id = "";
		_stream = nullptr;

		_protocol = "";
		_url = "";
		_stream_key = "";

		_Push_bytes = 0;
		_Push_time = 0;

		_Push_total_bytes = 0;
		_Push_total_time = 0;
		_sequence = 0;

		_state = PushState::Ready;
	}

	void Push::SetId(ov::String Push_id)
	{
		_id = Push_id;
	}

	ov::String Push::GetId() const
	{
		return _id;
	}
		
	void Push::SetEnable(bool eanble) 
	{ 
		_enable = eanble; 
	}
	bool Push::GetEnable() 
	{ 
		return _enable; 
	}

	void Push::SetVhost(ov::String value)
	{
		_vhost_name = value;
	}
	ov::String Push::GetVhost()
	{
		return _vhost_name;
	}

	void Push::SetApplication(ov::String value)
	{
		_aplication_name = value;
	}

	ov::String Push::GetApplication()
	{
		return _aplication_name;
	}

	void Push::SetRemove(bool value)
	{
		_remove = value;
	}

	bool Push::GetRemove()
	{
		return _remove;
	}

	ov::String Push::GetStreamName()
	{
		return _stream->GetName();
	}

	void Push::SetSessionId(session_id_t id)
	{
		_session_id = id;
	}

	session_id_t Push::GetSessionId() {
		return _session_id;
	}

	void Push::SetStream(const info::Stream &stream)
	{
		_stream = std::make_shared<info::Stream>(stream);		
	}

	const std::chrono::system_clock::time_point &Push::GetCreatedTime() const
	{
		return _created_time;
	}

	void Push::SetProtocol(ov::String protocol)
	{
		_protocol = protocol;
	}
	ov::String Push::GetProtocol()
	{
		return _protocol;
	}

	void Push::SetUrl(ov::String url)
	{
		_url = url;
	}
	ov::String Push::GetUrl()
	{
		return _url;
	}

	void Push::SetStreamKey(ov::String stream_key)
	{
		_stream_key = stream_key;
	}
	ov::String Push::GetStreamKey()
	{
		return _stream_key;
	}

	void Push::IncreasePushBytes(uint64_t bytes)
	{
		_Push_bytes += bytes;
	}
	void Push::UpdatePushTime()
	{
		_Push_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _Push_start_time).count();
	}
	void Push::IncreaseSequence()
	{
		_sequence++;
		_Push_total_bytes += _Push_bytes;
		_Push_bytes = 0;
		_Push_total_time += _Push_time;
		_Push_time = 0;
	}
	void Push::UpdatePushStartTime()
	{
		_Push_start_time = std::chrono::system_clock::now();
	}
	void Push::UpdatePushStopTime()
	{
		_Push_stop_time = std::chrono::system_clock::now();
	}

	uint64_t Push::GetPushBytes()
	{
		return _Push_bytes;
	}
	uint64_t Push::GetPushTime()
	{
		return _Push_time;
	}
	uint64_t Push::GetPushTotalBytes()
	{
		return _Push_total_bytes + _Push_bytes;
	}
	uint64_t Push::GetPushTotalTime()
	{
		return _Push_total_time + _Push_time;
	}
	uint64_t Push::GetSequence()
	{
		return _sequence;
	}
	const std::chrono::system_clock::time_point Push::GetPushStartTime() const
	{
		return _Push_start_time;
	}
	const std::chrono::system_clock::time_point Push::GetPushStopTime() const
	{
		return _Push_stop_time;
	}
	Push::PushState Push::GetState()
	{
		return _state;
	}
	void Push::SetState(Push::PushState state)
	{
		_state = state;
	}
	ov::String Push::GetStateString()
	{
		switch(GetState())
		{
			case PushState::Ready:
				return "ready";
			case PushState::Pushing:
				return "Pushing";				
			case PushState::Stopping:
				return "stopping";					
			case PushState::Stopped:
				return "stopped";	
			case PushState::Error:
				return "error";									
		}

		return "Unknown";
	}

	const ov::String Push::GetInfoString() 
	{
		ov::String info = "\n";

		info.AppendFormat(" id=%s\n", _id.CStr());
		info.AppendFormat(" stream=%s\n", (_stream != nullptr)?_stream->GetName().CStr():"");
		info.AppendFormat(" protocol=%s\n", _protocol.CStr());
		info.AppendFormat(" url=%s\n", _url.CStr());
		info.AppendFormat(" stream_key=%s\n", _stream_key.CStr());
		info.AppendFormat(" push_bytes=%lld\n", _Push_bytes);
		info.AppendFormat(" push_bytes=%lld\n", _Push_bytes);
		info.AppendFormat(" push_total_bytes=%lld\n", _Push_total_bytes);
		info.AppendFormat(" push_total_time=%lld\n", _Push_total_time);
		info.AppendFormat(" sequence=%d\n", _sequence);
		info.AppendFormat(" created_time=%s\n", ov::Converter::ToString(_created_time).CStr());
		info.AppendFormat(" Push_start_time=%s\n", ov::Converter::ToString(_Push_start_time).CStr());
		info.AppendFormat(" Push_stop_time=%s", ov::Converter::ToString(_Push_stop_time).CStr());

		return info;
	}	

}  // namespace info