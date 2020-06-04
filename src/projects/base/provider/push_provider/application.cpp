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

	bool PushApplication::JoinStream(const std::shared_ptr<PushStream> &stream)
	{
		stream->SetApplication(GetSharedPtrAs<Application>());
		stream->SetApplicationInfo(GetSharedPtrAs<Application>());
		
		if(GetStreamByName(stream->GetName()) != nullptr)
		{
			logti("Duplicate Stream Input(reject) - %s/%s", GetName().CStr(), stream->GetName().CStr());

			// TODO(Getroot): Write codes for block duplicate stream name configuration.
			return false;
		}

		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);
		_streams[stream->GetId()] = stream;
		streams_lock.unlock();
		
		if(stream->IsReadyToReceiveStreamData() == false)
		{
			logte("The stream(%s/%s) is not yet ready to be published.", GetName().CStr(), stream->GetName().CStr());
			return false;
		}

		NotifyStreamCreated(stream);

		return true;
	}

	bool PushApplication::DeleteAllStreams()
	{
		return Application::DeleteAllStreams();
	}
}