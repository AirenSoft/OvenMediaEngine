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
		// Check if same stream name is exist in this application or MediaRouter(may be created by another provider)
		if(GetStreamByName(stream->GetName()) != nullptr || IsExistingInboundStream(stream->GetName()) == true)
		{
			// If MPEG-2 TS client continuously shoots UDP packets, too many logs will be output. So, reduce the number of logs like this.
			if(stream->GetNumberOfAttempsToPublish() % 30 == 1)
			{
				logtw("Reject stream creation : there is already an incoming stream with the same name. (%s)", stream->GetName().CStr());
			}
			return false;
		}
		
		if(stream->IsReadyToReceiveStreamData() == false)
		{
			logte("The stream(%s/%s) is not yet ready to be published.", GetName().CStr(), stream->GetName().CStr());
			return false;
		}

		return AddStream(stream);
	}

	bool PushApplication::DeleteAllStreams()
	{
		return Application::DeleteAllStreams();
	}
}