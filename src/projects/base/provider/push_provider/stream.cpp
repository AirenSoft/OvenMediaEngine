//==============================================================================
//
//  PushStream
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "provider.h"
#include "application.h"
#include "stream.h"
#include "provider_private.h"

namespace pvd
{
	PushStream::PushStream(StreamSourceType source_type, uint32_t channel_id, const std::shared_ptr<PushProvider> &provider)
		: Stream(source_type)
	{
		SetId(channel_id);
		_channel_id = channel_id;
		_provider = provider;
	}

	uint32_t PushStream::GetChannelId()
	{
		return _channel_id;
	}

	uint32_t PushStream::GetRelatedChannelId()
	{
		return _related_channel_id;
	}

	void PushStream::SetRelatedChannelId(uint32_t related_channel_id)
	{
		_related_channel_id = related_channel_id;
	}

	void PushStream::UpdateLastReceivedTime()
	{
		_stop_watch.Update();
	}

	void PushStream::SetTimeoutSec(time_t seconds)
	{
		_stop_watch.Start();
		_timeout_sec = seconds;
	}
	
	bool PushStream::IsTimedOut()
	{
		if(_timeout_sec == 0)
		{
			return false;
		}

		return _stop_watch.IsElapsed(_timeout_sec * 1000);
	}

	time_t PushStream::GetElapsedSecSinceLastReceived()
	{
		return _stop_watch.Elapsed() / 1000;
	}

	bool PushStream::PublishInterleavedChannel(const info::VHostAppName &vhost_app_name)
	{
		if(_provider == nullptr)
		{
			return false;
		}

		_is_published = _provider->PublishInterleavedChannel(GetChannelId(), vhost_app_name, GetSharedPtrAs<PushStream>());

		return _is_published;
	}

	bool PushStream::PublishDataChannel(const info::VHostAppName &vhost_app_name, const std::shared_ptr<PushStream> &data_channel)
	{
		if(_provider == nullptr)
		{
			return false;
		}

		return _provider->PublishDataChannel(GetChannelId(), GetRelatedChannelId(), vhost_app_name, data_channel);
	}

	CheckSignatureResult PushStream::HandleSignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, std::shared_ptr<const SignedPolicy> &signed_policy)
	{
		if(_provider == nullptr)
		{
			return CheckSignatureResult::Error;
		}

		return _provider->HandleSignedPolicy(request_url, client_address, signed_policy);
	}

	bool PushStream::DoesBelongApplication()
	{
		return GetApplication() != nullptr;
	}

	bool PushStream::IsPublished()
	{
		return _is_published;
	}

	bool PushStream::IsReadyToReceiveStreamData()
	{
		// Check if it is signalling channel
		if(GetPushStreamType() == PushStreamType::SIGNALLING || GetPushStreamType() == PushStreamType::UNKNOWN)
		{
			return false;
		}

		// Check if it belongs application
		if(DoesBelongApplication() == false)
		{
			return false;
		}

		// Check if it has stream name
		if(GetName().GetLength() == 0)
		{
			return false;
		}

		// Check if it has track information
		if(GetTracks().size() == 0)
		{
			return false;
		}

		return true;
	}
}