#include "publisher_private.h"
#include "session.h"
#include "application.h"

Session::Session(const std::shared_ptr<Application> &application, const std::shared_ptr<Stream> &stream)
	: SessionInfo()
{
	_application = application;
	_stream = stream;
	_state = SessionState::Ready;
}

Session::Session(const SessionInfo &info, const std::shared_ptr<Application> &application, const std::shared_ptr<Stream> &stream)
	: SessionInfo(info)
{
	_application = application;
	_stream = stream;
	_state = SessionState::Ready;
}

Session::~Session()
{
}

const std::shared_ptr<Application>& Session::GetApplication()
{
	return _application;
}

const std::shared_ptr<Stream>& Session::GetStream()
{
	return _stream;
}

bool Session::Start()
{
	_state = SessionState::Started;
	return true;
}

bool Session::Stop()
{
	// Virtual Function으로 자식 class는 이미 모든 정리를 마무리 했다.
	// 상태를 변경한다.
	_state = SessionState::Stopped;

	return true;
}

Session::SessionState Session::GetState()
{
	return _state;
}

void Session::Terminate(ov::String reason)
{
	_state = SessionState::Error;
	_error_reason = reason;

	GetStream()->RemoveSession(GetId());
}