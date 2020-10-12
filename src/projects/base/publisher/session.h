//
// Created by getroot on 18. 3. 19.
//

#pragma once

#include "base/common_types.h"
#include "base/info/session.h"

#include <base/ovlibrary/ovlibrary.h>

namespace pub
{
	class Application;
	class Stream;

	class Session : public info::Session
	{
	public:
		Session(const std::shared_ptr<Application> &application, const std::shared_ptr<Stream> &stream);
		Session(const info::Session &info, const std::shared_ptr<Application> &app, const std::shared_ptr<Stream> &stream);
		virtual ~Session();

		const std::shared_ptr<Application> &GetApplication();
		const std::shared_ptr<Stream> &GetStream();

		virtual bool Start();
		virtual bool Stop();
		
		virtual bool SendOutgoingData(const std::any &packet) = 0;
		virtual void OnPacketReceived(const std::shared_ptr<info::Session> &session_info, const std::shared_ptr<const ov::Data> &data) = 0;

		enum class SessionState : int8_t
		{
			Ready,
			Started,
			Stopped,
			Error
		};

		SessionState GetState();
		virtual void Terminate(ov::String reason);

	private:
		std::shared_ptr<Application> _application;
		std::shared_ptr<Stream> _stream;
		SessionState _state;
		ov::String _error_reason;
	};

}  // namespace pub