
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

	// To be interleaved mode, a channel must have applicaiton/stream and track informaiton
	bool PushProvider::PublishInterleavedChannel(uint32_t channel_id, ov::String app_name, const std::shared_ptr<PushStream> &signal_channel)
	{
		// Append the stream into the application
		auto application = std::dynamic_pointer_cast<PushApplication>(GetApplicationByName(app_name));
		if(application == nullptr)
		{
			logte("Cannot find application(%s) to publish interleaved channel", app_name.CStr());
			return false;
		}

		application->JoinStream(signal_channel);

		return false;
	}

	// A data channel must have applicaiton/stream and track informaiton
	bool PushProvider::PublishDataChannel(uint32_t channel_id, uint32_t signalling_channel_id, ov::String app_name, const std::shared_ptr<pvd::PushStream> &channel)
	{
		// Create stream
		channel->SetRelatedChannelId(signalling_channel_id);

		auto application = GetApplicationByName(app_name);
		if(application == nullptr)
		{
			logte("Cannot find %s application to create %s stream", app_name.CStr(), channel->GetName().CStr());
		}

		// Notify stream created 

		return false;
	}

	bool PushProvider::OnSignallingChannelCreated(uint32_t channel_id, const std::shared_ptr<pvd::PushStream> &channel)
	{
		std::lock_guard<std::shared_mutex> lock(_channels_lock);

		_channels.emplace(channel_id, channel);

		return true;
	}

	bool PushProvider::OnDataReceived(uint32_t channel_id, const std::shared_ptr<const ov::Data> &data)
	{
		std::shared_lock<std::shared_mutex> lock(_channels_lock);
		// find stream by client_id
		auto it = _channels.find(channel_id);
		if(it == _channels.end())
		{
			return false;
		}

		// send data 
		auto channel = it->second;

		// In the future, 
		// it may be necessary to send data to an application rather than sending it directly to a stream.
		channel->OnDataReceived(data);

		return true;
	}

	bool PushProvider::OnChannelDeleted(uint32_t channel_id, const std::shared_ptr<const ov::Error> &error)
	{
		// Delete from stream_mold
		std::unique_lock<std::shared_mutex> lock(_channels_lock);

		auto it = _channels.find(channel_id);
		if(it != _channels.end())
		{
			_channels.erase(it);
		}
		else
		{
			// Something wrong
			logte("%d channel to be deleted cannot be found", channel_id);
		}

		lock.unlock();

		// Delete from Application
		auto stream = it->second;

		// get stream type
		if(stream->DoesBelongApplication())
		{
			auto application = stream->GetApplication();
			if(application == nullptr)
			{
				// Something wrong
				logte("%d stream mold to be deleted cannot be found", channel_id);
				return false;
			}

			application->DeleteStream(stream);
		}

		return true;
	}
}