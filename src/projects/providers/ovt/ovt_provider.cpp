//
// Created by getroot on 19. 12. 9.
//

#include <base/ovlibrary/url.h>
#include "ovt_provider.h"
#include "ovt_application.h"
#include "ovt_stream.h"

#define OV_LOG_TAG "OvtProvider"

using namespace common;

namespace pvd
{
	std::shared_ptr<OvtProvider> OvtProvider::Create(const cfg::Server &server_config, const info::Host &host_info,
													 const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<OvtProvider>(server_config, host_info, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating RtmpProvider");
			return nullptr;
		}
		return provider;
	}

	OvtProvider::OvtProvider(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
			: Provider(server_config, host_info, router)
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

	bool OvtProvider::CheckOriginAvailability(const std::vector<ov::String> &url_list)
	{
		return true;
	}

	// Pull Stream
	bool OvtProvider::PullStream(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list)
	{
		// Find App
		auto app = std::dynamic_pointer_cast<OvtApplication>(GetApplicationById(app_info.GetId()));
		
		if (app == nullptr)
		{
			logte("There is no such app (%s)", app_info.GetName().CStr());
			return false;
		}

		// Find Stream (The stream must not exist)
		auto stream = app->GetStreamByName(stream_name);
		if (stream != nullptr)
		{
			// If stream is not running it can be deleted.
			if(stream->GetState() == Stream::State::STOPPED)
			{
				app->NotifyStreamDeleted(stream);
			}
			else
			{
				logte("%s stream already exists.", stream_name.CStr());
				return false;
			}
		}

		// Create Stream
		stream = app->CreateStream(stream_name, url_list);
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