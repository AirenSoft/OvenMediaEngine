#include "rtc_private.h"
#include "rtc_application.h"

#include "modules/ice/ice_port_manager.h"


std::shared_ptr<RtcApplication> RtcApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, 
													   const info::Application &application_info,
                                                       const std::shared_ptr<IcePort> &ice_port,
                                                       const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
{
	auto application = std::make_shared<RtcApplication>(publisher, application_info, ice_port, rtc_signalling);
	application->Start();
	return application;
}

RtcApplication::RtcApplication(const std::shared_ptr<pub::Publisher> &publisher, 
							   const info::Application &application_info,
                               const std::shared_ptr<IcePort> &ice_port,
                               const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
	: Application(publisher, application_info)
{
	_ice_port = ice_port;
	_rtc_signalling = rtc_signalling;
}

RtcApplication::~RtcApplication()
{
	Stop();
	logtd("RtcApplication(%d) has been terminated finally", GetId());
}

std::shared_ptr<Certificate> RtcApplication::GetCertificate()
{
	return _certificate;
}

std::shared_ptr<pub::Stream> RtcApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	// Stream Class 생성할때는 복사를 사용한다.
	logtd("RtcApplication::CreateStream : %s/%u", info->GetName().CStr(), info->GetId());
	return RtcStream::Create(GetSharedPtrAs<pub::Application>(), *info, worker_count);
}

bool RtcApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<RtcStream>(GetStream(info->GetId()));
	if(stream == nullptr)
	{
		logte("RtcApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	// 모든 Session의 연결을 종료한다.
	const auto sessions = stream->GetAllSessions();
	for(auto it = sessions.begin(); it != sessions.end();)
	{
		auto session = std::static_pointer_cast<RtcSession>(it->second);
		it++;

		// IcePort에 모든 Session 삭제
		_ice_port->RemoveSession(session);

		// Signalling에 모든 Session 삭제
		_rtc_signalling->Disconnect(GetName(), stream->GetName(), session->GetPeerSDP());
	}

	logtd("RtcApplication %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}

bool RtcApplication::Start()
{
	if(_certificate == nullptr)
	{
		// 인증서를 생성한다.
		_certificate = std::make_shared<Certificate>();

		auto error = _certificate->Generate();
		if(error != nullptr)
		{
			logte("Cannot create certificate: %s", error->ToString().CStr());
			return false;
		}
	}

	return Application::Start();
}

bool RtcApplication::Stop()
{
	return Application::Stop();
}