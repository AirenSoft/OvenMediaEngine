//
// Created by getroot on 19. 12. 9.
//

#include "ovt_application.h"
#include "ovt_stream.h"

#define OV_LOG_TAG "OvtApplication"

namespace pvd
{
	std::shared_ptr<OvtApplication> OvtApplication::Create(const info::Application &application_info)
	{
		auto application = std::make_shared<OvtApplication>(application_info);

		application->Start();

		return application;
	}

	OvtApplication::OvtApplication(const info::Application &info)
			: Application(info)
	{

	}

	OvtApplication::~OvtApplication()
	{

	}

	std::shared_ptr<pvd::Stream> OvtApplication::CreateStream(const std::shared_ptr<const ov::Url> &url)
	{
		logtd("OnCreateStream");
		auto stream = OvtStream::Create(GetSharedPtrAs<pvd::Application>(), url);
		return stream;
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