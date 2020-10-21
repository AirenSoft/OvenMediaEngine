#include "session.h"
#include "application.h"
#include "base/info/stream.h"
#include "publisher_private.h"

namespace pub
{
	Session::Session(const std::shared_ptr<Application> &application, const std::shared_ptr<Stream> &stream)
		: info::Session(*std::static_pointer_cast<info::Stream>(stream))
	{
		_application = application;
		_stream = stream;
		_state = SessionState::Ready;
	}

	Session::Session(const info::Session &info, const std::shared_ptr<Application> &application, const std::shared_ptr<Stream> &stream)
		: info::Session(*std::static_pointer_cast<info::Stream>(stream), info)
	{
		_application = application;
		_stream = stream;
		_state = SessionState::Ready;
	}

	Session::~Session()
	{
	}

	const std::shared_ptr<Application> &Session::GetApplication()
	{
		return _application;
	}

	const std::shared_ptr<Stream> &Session::GetStream()
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
		_state = SessionState::Stopped;

		return true;
	}

	Session::SessionState Session::GetState()
	{
		return _state;
	}

	void Session::SetState(SessionState state)
	{
		_state = state;
	}

	void Session::Terminate(ov::String reason)
	{
		_state = SessionState::Error;
		_error_reason = reason;

		GetStream()->RemoveSession(GetId());
	}
}  // namespace pub