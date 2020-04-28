//
// Created by getroot on 19. 12. 9.
//

#include "rtspc_application.h"
#include "rtspc_stream.h"

#define OV_LOG_TAG "RtspcApplication"

namespace pvd
{
	std::shared_ptr<RtspcApplication> RtspcApplication::Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
	{
		auto application = std::make_shared<RtspcApplication>(provider, application_info);

		application->Start();

		return application;
	}

	RtspcApplication::RtspcApplication(const std::shared_ptr<Provider> &provider, const info::Application &info)
			: Application(provider, info)
	{

	}

	RtspcApplication::~RtspcApplication()
	{

	}

	std::shared_ptr<pvd::Stream> RtspcApplication::CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list)
	{
		return RtspcStream::Create(GetSharedPtrAs<pvd::Application>(), stream_id, stream_name, url_list);
	}

	bool RtspcApplication::Start()
	{
		return pvd::Application::Start();
	}

	bool RtspcApplication::Stop()
	{
		return pvd::Application::Stop();
	}
}