//
// Created by getroot on 19. 12. 9.
//

#include "ovt_application.h"
#include "ovt_stream.h"

#define OV_LOG_TAG "OvtApplication"

namespace pvd
{
	std::shared_ptr<OvtApplication> OvtApplication::Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
	{
		auto application = std::make_shared<OvtApplication>(provider, application_info);

		application->Start();

		return application;
	}

	OvtApplication::OvtApplication(const std::shared_ptr<Provider> &provider, const info::Application &info)
			: Application(provider, info)
	{

	}

	OvtApplication::~OvtApplication()
	{

	}

	std::shared_ptr<pvd::Stream> OvtApplication::CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list)
	{
		return OvtStream::Create(GetSharedPtrAs<pvd::Application>(), stream_id, stream_name, url_list);
	}

	bool OvtApplication::Start()
	{
		return pvd::Application::Start();
	}

	bool OvtApplication::Stop()
	{
		return pvd::Application::Stop();
	}
}