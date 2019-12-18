//
// Created by getroot on 19. 12. 9.
//

#include <base/ovlibrary/url.h>
#include "ovt_provider.h"
#include "ovt_application.h"

#define OV_LOG_TAG "OvtProvider"

using namespace common;

namespace pvd
{
	std::shared_ptr<OvtProvider> OvtProvider::Create(const info::Host &host_info,
													 const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<OvtProvider>(host_info, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating RtmpProvider");
			return nullptr;
		}
		return provider;
	}

	OvtProvider::OvtProvider(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
			: Provider(host_info, router)
	{

	}

	OvtProvider::~OvtProvider()
	{
		Stop();
		logtd("Terminated OvtProvider modules.");
	}

	bool OvtProvider::Start()
	{
		return pvd::Provider::Start();
	}

	bool OvtProvider::Stop()
	{
		return pvd::Provider::Stop();
	}

	bool OvtProvider::CheckOriginsAvailability(const std::vector<ov::String> &url_list)
	{
		return true;
	}

	// Pull Stream
	bool OvtProvider::PullStreams(info::application_id_t app_id, const ov::String &app_name, const ov::String stream_name, const std::vector<ov::String> &url_list)
	{
		// Dummy code
		auto url = url_list[0];

		auto url_parser = ov::Url::Parse(url.CStr());
		// auto app_name = url_parser->App();
		// auto stream_name = url_parser->Stream();

		// Find App
		auto app = std::dynamic_pointer_cast<OvtApplication>(GetApplicationByName(app_name));
		if (app == nullptr)
		{
			logte("There is no such app (%s)", app_name.CStr());
			return false;
		}

		// Find Stream (The stream must not exist)
		auto stream = app->GetStreamByName(stream_name);
		if (stream != nullptr)
		{
			logte("%s stream already exists.", stream_name.CStr());
			return false;
		}

		// Create Stream
		stream = app->CreateStream(url_parser);
		if (stream == nullptr)
		{
			logte("Cannot create %s stream.", stream_name.CStr());
			return false;
		}

		// Notify stream has been created
		app->NotifyStreamCreated(stream);

		return true;
	}

	std::shared_ptr<pvd::Application> OvtProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		return OvtApplication::Create(app_info);
	}

	bool OvtProvider::OnDeleteProviderApplication(const info::Application &app_info)
	{

		return true;
	}
}