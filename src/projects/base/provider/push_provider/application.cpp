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
			logti("Reject %s/%s stream it is a stream with a duplicate name.", GetName().CStr(), stream->GetName().CStr());		
			return false;
		}
	
		if(stream->IsReadyToReceiveStreamData() == false)
		{
			logte("The stream(%s/%s) is not yet ready to be published.", GetName().CStr(), stream->GetName().CStr());
			return false;
		}

		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);
		_streams[stream->GetId()] = stream;
		streams_lock.unlock();

		NotifyStreamCreated(stream);

		return true;
	}

	bool PushApplication::DeleteAllStreams()
	{
		return Application::DeleteAllStreams();
	}
}