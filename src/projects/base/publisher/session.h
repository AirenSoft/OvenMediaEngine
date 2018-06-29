//
// Created by getroot on 18. 3. 19.
//

#pragma once

#include "base/common_video.h"
#include "base/ovlibrary/data.h"
#include "base/application/session_info.h"
#include "base/ovlibrary/data.h"

class Application;
class Stream;

class Session : public SessionInfo
{
public:
	Session(std::shared_ptr<Application> application, std::shared_ptr<Stream> stream);
	Session(const SessionInfo &info, std::shared_ptr<Application> app, std::shared_ptr<Stream> stream);
	virtual ~Session() = default;

	std::shared_ptr<Application>		GetApplication();
	std::shared_ptr<Stream>				GetStream();

	virtual bool	Start();
	virtual bool	Stop();

	// 상위 Layer에서 Packet을 수신받는다.
	virtual void	OnPacketReceived(std::shared_ptr<SessionInfo> session_info, std::shared_ptr<const ov::Data> data) = 0;

	enum class SessionState
	{
		Ready,
		Started,
		Stopped,
		Error
	};

	SessionState	GetState();
	virtual void	Terminate(ov::String reason);

private:
	std::shared_ptr<Application>		_application;
	std::shared_ptr<Stream>				_stream;
	SessionState						_state;
	ov::String							_error_reason;
};

