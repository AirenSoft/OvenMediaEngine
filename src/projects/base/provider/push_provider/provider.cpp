
#include "provider.h"
#include "provider_private.h"

namespace pvd
{
	PushProvider::PushProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Provider(server_config, router)
	{
	}

	PushProvider::~PushProvider()
	{

	}

    bool PushProvider::Start()
    {
        return Provider::Start();
    }

	bool PushProvider::Stop()
    {
        return Provider::Stop();
    }

	bool PushProvider::AssignStream(ov::String app_name, std::shared_ptr<PushStream> &stream)
	{
		return true;
	}

	bool PushProvider::OnConnected(uint32_t client_id)
	{
		auto stream_mold = CreateStreamMold();
		if(stream_mold == nullptr)
		{
			logte("Cannot create stream mold (%d) for %s", GetProviderName());
			return false;
		}

		return true;
	}

	bool PushProvider::OnDataReceived(uint32_t client_id, const std::shared_ptr<const ov::Data> &data)
	{
		return true;
	}

	bool PushProvider::OnDisconnected(uint32_t client_id, const std::shared_ptr<const ov::Error> &error)
	{
		return true;
	}

}