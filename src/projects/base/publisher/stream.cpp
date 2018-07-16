#include "publisher_private.h"
#include "stream.h"

Stream::Stream(const std::shared_ptr<Application> application,
               const StreamInfo &info)
	: StreamInfo(info)
{
	_application = application;
}

Stream::~Stream()
{
	Stop();
}

bool Stream::Start()
{
	return true;
}

bool Stream::Stop()
{
	// Session을 모두 정리한다.

	for(auto &t :_sessions)
	{
		t.second->Stop();
	}

	_sessions.clear();

	return true;
}

std::shared_ptr<Application> Stream::GetApplication()
{
	return _application;
}

const std::map<session_id_t, std::shared_ptr<Session>> &Stream::GetSessionMap()
{
	std::unique_lock<std::mutex> lock(_session_map_guard);
	return _sessions;
}

bool Stream::AddSession(std::shared_ptr<Session> session)
{
	std::unique_lock<std::mutex> lock(_session_map_guard);
	_sessions[session->GetId()] = session;
	return true;
}


bool Stream::RemoveSession(session_id_t id)
{
	std::unique_lock<std::mutex> lock(_session_map_guard);
	if(_sessions.count(id) <= 0)
	{
		logtd("Cannot find session : %u", id);
		return false;
	}

	auto session = _sessions[id];
	// Session에 더이상 패킷을 전달하지 않는 것이 먼저다.
	_sessions.erase(id);

	// Session 동작을 중지한다.
	session->Stop();

	logtd("Session deleted : %u", id);

	return true;
}

std::shared_ptr<Session> Stream::GetSession(session_id_t id)
{
	if(_sessions.count(id) <= 0)
	{
		return nullptr;
	}

	return _sessions[id];
}