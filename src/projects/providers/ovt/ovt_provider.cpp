//
// Created by getroot on 19. 12. 9.
//

#include "ovt_provider.h"

#define OV_LOG_TAG "OvtProvider"

using namespace common;

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
	// Create PhysicalPort

	return true;
}

bool OvtProvider::Stop()
{


	return true;
}

// Pull Stream
bool OvtProvider::PullStream(ov::String app_name, ov::String stream_name, ov::String url)
{

	return true;
}

std::shared_ptr<pvd::Application> OvtProvider::OnCreateProviderApplication(const info::Application &application_info)
{

	//return OvtApplication::Create(application_info);
}