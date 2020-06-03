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
	PushStream::PushStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		: Stream(application, stream_info)
	{	
	}
	PushStream::PushStream(StreamSourceType source_type, uint32_t channel_id, const std::shared_ptr<PushProvider> &provider)
		: Stream(source_type)
	{
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

	bool PushStream::PublishInterleavedChannel(ov::String app_name)
	{
		if(_provider == nullptr)
		{
			return false;
		}

		return _provider->PublishInterleavedChannel(GetChannelId(), app_name, GetSharedPtrAs<PushStream>());
	}

	bool PushStream::PublishDataChannel(ov::String app_name, const std::shared_ptr<PushStream> &data_channel)
	{
		if(_provider == nullptr)
		{
			return false;
		}

		return _provider->PublishDataChannel(GetChannelId(), GetRelatedChannelId(), app_name, data_channel);
	}

	bool PushStream::DoesBelongApplication()
	{
		return GetApplication() != nullptr;
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