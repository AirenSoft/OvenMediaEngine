#include "rtc_private.h"
#include "rtc_application.h"

#include "modules/ice/ice_port_manager.h"


std::shared_ptr<RtcApplication> RtcApplication::Create(const info::Application &application_info,
                                                       const std::shared_ptr<IcePort> &ice_port,
                                                       const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
{
	auto application = std::make_shared<RtcApplication>(application_info, ice_port, rtc_signalling);
	application->Start();
	return application;
}

RtcApplication::RtcApplication(const info::Application &application_info,
                               const std::shared_ptr<IcePort> &ice_port,
                               const std::shared_ptr<RtcSignallingServer> &rtc_signalling)
	: Application(application_info)
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

std::shared_ptr<pub::Stream> RtcApplication::CreateStream(const std::shared_ptr<info::StreamInfo> &info, uint32_t worker_count)
{
	// Stream Class 생성할때는 복사를 사용한다.
	logtd("RtcApplication::CreateStream : %s/%u", info->GetName().CStr(), info->GetId());
	if(worker_count == 0)
	{
		// RtcStream should have worker threads.
		worker_count = MIN_STREAM_THREAD_COUNT;
	}
	return RtcStream::Create(GetSharedPtrAs<pub::Application>(), *info, worker_count);
}

bool RtcApplication::DeleteStream(const std::shared_ptr<info::StreamInfo> &info)
{
	logtd("DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<RtcStream>(GetStream(info->GetId()));
	if(stream == nullptr)
	{
		logte("RtcApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	// 모든 Session의 연결을 종료한다.
	const auto &sessions = stream->GetAllSessions();
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
	auto publisher_info = GetPublisher<cfg::WebrtcPublisher>();
	if(!publisher_info->IsParsed())
	{
		// There is no config for webrtc, it means admin doesn't want to enable webrtc
		return false;
	}

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

		logti("WebRTC Application [%s] Started", GetName().CStr());
	}

	return Application::Start();
}

bool RtcApplication::Stop()
{
	// TODO(dimiden): Application이 종료되는 경우는 향후 Application 단위의 재시작 기능이 개발되면 필요함
	// 그 전에는 프로그램 종료까지 본 함수는 호출되지 않음
	return Application::Stop();
}

// RTCP RR packet info
// call from stream -> session -> RtpRtcp
// packetizer checkr check ssrc_1(video/audio)
void RtcApplication::OnReceiverReport(uint32_t stream_id,
                                      uint32_t session_id,
                                      time_t first_receiver_report_time,
                                      const std::shared_ptr<RtcpReceiverReport> &receiver_report)
{
    logtd("Rtcp Report: app(%u) stream(%u) session(%u) ssrc(%u) fraction(%u/256) packet_lost(%d) jitter(%u) delay(%.6f)",
          GetId(),
          stream_id,
          session_id,
          receiver_report->ssrc_1,
          receiver_report->fraction_lost,
          receiver_report->packet_lost,
          receiver_report->jitter,
          receiver_report->rtt);
}