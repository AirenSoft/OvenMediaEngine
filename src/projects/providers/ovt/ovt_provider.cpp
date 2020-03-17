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
	std::shared_ptr<OvtProvider> OvtProvider::Create(const cfg::Server &server_config,
													 const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<OvtProvider>(server_config, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating OvtProvider");
			return nullptr;
		}
		return provider;
	}

	OvtProvider::OvtProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
			: Provider(server_config, router)
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

	// Pull Stream
	std::shared_ptr<pvd::Stream> OvtProvider::PullStream(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset)
	{
		// Find App
		auto app = std::dynamic_pointer_cast<OvtApplication>(GetApplicationById(app_info.GetId()));
		if (app == nullptr)
		{
			logte("There is no such app (%s)", app_info.GetName().CStr());
			return nullptr;
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
				return nullptr;
			}
		}

		// Create Stream
		stream = app->CreateStream(stream_name, url_list);
		if (stream == nullptr)
		{
			logte("Cannot create %s stream.", stream_name.CStr());
			return nullptr;
		}

		// Notify stream has been created
		app->NotifyStreamCreated(stream);

		return stream;
	}

	bool OvtProvider::StopStream(const info::Application &app_info, const std::shared_ptr<pvd::Stream> &stream)
	{
		return stream->Stop();
	}

	std::shared_ptr<pvd::Application> OvtProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		return OvtApplication::Create(app_info);
	}

	bool OvtProvider::OnDeleteProviderApplication(const info::Application &app_info)
	{
		return true;
	}

	void OvtProvider::OnStreamNotInUse(const info::Stream &stream_info)
	{
		logti("%s stream will be deleted because it is not used", stream_info.GetName().CStr());

		// Find App
		auto app_info = stream_info.GetApplicationInfo();
		auto app = std::dynamic_pointer_cast<OvtApplication>(GetApplicationById(app_info.GetId()));
		if (app == nullptr)
		{
			logte("There is no such app (%s)", app_info.GetName().CStr());
			return;
		}

		// Find Stream (The stream must not exist)
		auto stream = app->GetStreamById(stream_info.GetId());
		if (stream == nullptr)
		{
			logte("There is no such stream (%s)", app_info.GetName().CStr());
			return;
		}

		StopStream(app_info, stream);
	}
}