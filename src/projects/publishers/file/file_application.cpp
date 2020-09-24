#include "file_private.h"
#include "file_application.h"
#include "file_stream.h"
#include "file_session.h"

std::shared_ptr<FileApplication> FileApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<FileApplication>(publisher, application_info);
	application->Start();
	return application;
}

FileApplication::FileApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
		: Application(publisher, application_info)
{

}

FileApplication::~FileApplication()
{
	Stop();
	logtd("FileApplication(%d) has been terminated finally", GetId());
}

bool FileApplication::Start()
{
	return Application::Start();
}

bool FileApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<pub::Stream> FileApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("FileApplication::CreateStream : %s/%u", info->GetName().CStr(), info->GetId());
	return FileStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool FileApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("FileApplication::DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<FileStream>(GetStream(info->GetId()));
	if(stream == nullptr)
	{
		logte("FileApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}
	
	logtd("FileApplication %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}


