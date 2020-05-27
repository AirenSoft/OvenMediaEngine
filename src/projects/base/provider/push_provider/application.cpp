//==============================================================================
//
//  PushProviderApplication 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "application.h"
#include "provider.h"
#include "provider_private.h"

namespace pvd
{
	PushApplication::PushApplication(const std::shared_ptr<PushProvider> &provider, const info::Application &application_info)
		: Application(provider, application_info)
	{
	}

	std::shared_ptr<pvd::PushStream> PushApplication::CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks)
	{
		// Get stream from child
		auto stream = CreateStream(stream_id, stream_name);
		if(stream == nullptr)
		{
			return nullptr;
		}

		for(const auto &track : tracks)
		{
			stream->AddTrack(track);
		}
	
		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);
		_streams[stream->GetId()] = stream;
		streams_lock.unlock();

		NotifyStreamCreated(stream);

		return stream;
	}
}