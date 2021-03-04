
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
	bool PushProvider::PublishChannel(uint32_t channel_id, const info::VHostAppName &vhost_app_name, const std::shared_ptr<PushStream> &channel)
	{
		// Append the stream into the application
		auto application = std::dynamic_pointer_cast<PushApplication>(GetApplicationByName(vhost_app_name));
		if(application == nullptr)
		{
			logte("Cannot find application(%s) to publish interleaved channel", vhost_app_name.CStr());
			return false;
		}

		return application->JoinStream(channel);
	}

	std::shared_ptr<PushStream> PushProvider::GetChannel(uint32_t channel_id)
	{
		std::shared_lock<std::shared_mutex> lock(_channels_lock);
		// find stream by client_id
		auto it = _channels.find(channel_id);
		if(it == _channels.end())
		{
			return nullptr;
		}

		return it->second;
	}

	bool PushProvider::OnChannelCreated(uint32_t channel_id, const std::shared_ptr<pvd::PushStream> &channel)
	{
		std::lock_guard<std::shared_mutex> lock(_channels_lock);

		_channels.emplace(channel_id, channel);

		return true;
	}

	bool PushProvider::OnDataReceived(uint32_t channel_id, const std::shared_ptr<const ov::Data> &data)
	{
		auto channel = GetChannel(channel_id);
		if(channel == nullptr)
		{
			return false;
		}

		// In the future, 
		// it may be necessary to send data to an application rather than sending it directly to a stream.
		if(channel->OnDataReceived(data) == true)
		{
			channel->UpdateLastReceivedTime();
		}

		return true;
	}

	bool PushProvider::OnChannelDeleted(const std::shared_ptr<pvd::PushStream> &channel)
	{
		if(channel == nullptr)
		{
			return false;
		}

		SetChannelTimeout(channel, 0);

		// Delete from stream_mold
		std::unique_lock<std::shared_mutex> lock(_channels_lock);
		
		if(_channels.erase(channel->GetChannelId()) == 0)
		{
			// Something wrong
			logte("%d channel to be deleted cannot be found", channel->GetChannelId());
			return false;
		}

		lock.unlock();

		// Delete from Application
		if(channel->DoesBelongApplication())
		{
			auto application = channel->GetApplication();
			if(application == nullptr)
			{
				// Something wrong
				logte("Cannot find application to delete stream (%s)", channel->GetName().CStr());
				return false;
			}

			application->DeleteStream(channel);
		}

		return true;
	}

	bool PushProvider::OnChannelDeleted(uint32_t channel_id)
	{
		return OnChannelDeleted(GetChannel(channel_id));
	}

	bool PushProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		std::unique_lock<std::shared_mutex> lock(_channels_lock);

		auto it = _channels.begin();
		while(it != _channels.end())
		{
			auto channel = it->second;

			if(channel->GetApplication() == nullptr)
			{
				it ++;
				continue;
			}

			if(channel->GetApplication()->GetId() == application->GetId())
			{
				it = _channels.erase(it);
			}
			else
			{
				it ++;
			}
			
		}
		return true;
	}

	bool PushProvider::StartTimer()
	{
		_stop_timer_thread_flag = false;
		_timer_thread = std::thread(&PushProvider::TimerThread, this);
		pthread_setname_np(_timer_thread.native_handle(), "PProviderTimer");

		return true;
	}

	bool PushProvider::StopTimer()
	{
		_stop_timer_thread_flag = true;
		if(_timer_thread.joinable())
		{
			_timer_thread.join();
		}

		return true;
	}

	void PushProvider::OnTimer(const std::shared_ptr<PushStream> &channel)
	{
		// For child function
	}

	void PushProvider::SetChannelTimeout(const std::shared_ptr<PushStream> &channel, time_t seconds)
	{
		channel->SetTimeoutSec(seconds);
	}

	void PushProvider::TimerThread()
	{
		std::shared_lock<std::shared_mutex> lock(_channels_lock, std::defer_lock);

		while(_stop_timer_thread_flag == false)
		{
			lock.lock();
			auto channels = _channels;
			lock.unlock();

			for(const auto &x : channels)
			{
				auto channel = x.second;
				if(channel->IsTimedOut())
				{
					OnTimer(channel);
				}
			}

			sleep(1);
		}
	}
}