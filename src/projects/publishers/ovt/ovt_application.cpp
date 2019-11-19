
#include "ovt_private.h"
#include "ovt_application.h"

std::shared_ptr<OvtApplication> OvtApplication::Create(const info::Application &application_info)
{
	auto application = std::make_shared<OvtApplication>(application_info);
	application->Start();
	return application;
}

OvtApplication::OvtApplication(const info::Application &application_info)
		: Application(application_info)
{

}

OvtApplication::~OvtApplication()
{
	Stop();
	logtd("OvtApplication(%d) has been terminated finally", GetId());
}

bool OvtApplication::Start()
{
	logti("Ovt Application [%s] Started", GetName().CStr());

	return Application::Start();
}

bool OvtApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<Stream> OvtApplication::CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count)
{
	logtd("OvtApplication::CreateStream : %s/%u", info->GetName().CStr(), info->GetId());
	if(worker_count == 0)
	{
		// RtcStream should have worker threads.
		worker_count = MIN_STREAM_THREAD_COUNT;
	}
	return OvtStream::Create(GetSharedPtrAs<Application>(), *info, worker_count);
}

bool OvtApplication::DeleteStream(std::shared_ptr<StreamInfo> info)
{
	// Input이 종료된 경우에 호출됨, 이 경우에는 Stream을 삭제 해야 하고, 그 전에 연결된 모든 Session을 종료
	// StreamInfo로 Stream을 구한다.
	logtd("OvtApplication::DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<OvtStream>(GetStream(info->GetId()));
	if(stream == nullptr)
	{
		logte("Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	// 모든 Session의 연결을 종료한다.
	logti("OvtApplication %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}