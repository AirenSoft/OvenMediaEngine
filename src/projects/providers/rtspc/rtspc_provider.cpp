//
// Created by getroot on 19. 12. 9.
//

#include <base/ovlibrary/url.h>
#include "rtspc_provider.h"
#include "rtspc_application.h"
#include "rtspc_stream.h"

#define OV_LOG_TAG "RtspcProvider"

using namespace common;

namespace pvd
{
	std::shared_ptr<RtspcProvider> RtspcProvider::Create(const cfg::Server &server_config,
													 const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<RtspcProvider>(server_config, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating RtspcProvider");
			return nullptr;
		}
		return provider;
	}

	RtspcProvider::RtspcProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
			: Provider(server_config, router)
	{
		logtd("Created Rtspc Provider module.");
	}

	RtspcProvider::~RtspcProvider()
	{
		Stop();
		logtd("Terminated Rtspc Provider modules.");
	}

	bool RtspcProvider::Start()
	{
		return pvd::Provider::Start();
	}

	bool RtspcProvider::Stop()
	{
		return pvd::Provider::Stop();
	}

	// Pull Stream
	std::shared_ptr<pvd::Stream> RtspcProvider::PullStream(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset)
	{
		// Find App
		auto app = std::dynamic_pointer_cast<RtspcApplication>(GetApplicationById(app_info.GetId()));
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

	bool RtspcProvider::StopStream(const info::Application &app_info, const std::shared_ptr<pvd::Stream> &stream)
	{
		return stream->Stop();
	}

	std::shared_ptr<pvd::Application> RtspcProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		return RtspcApplication::Create(app_info);
	}

	bool RtspcProvider::OnDeleteProviderApplication(const info::Application &app_info)
	{

		// TODO(soulk) : application deletion function should be developed.
		return true;
	}

	void RtspcProvider::OnStreamNotInUse(const info::Stream &stream_info)
	{
		return;
		logti("%s stream will be deleted becase it is not used", stream_info.GetName().CStr());

		// Find App
		auto app_info = stream_info.GetApplicationInfo();
		auto app = std::dynamic_pointer_cast<RtspcApplication>(GetApplicationById(app_info.GetId()));
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