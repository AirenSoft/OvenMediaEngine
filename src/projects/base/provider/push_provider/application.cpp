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
		if(stream->IsReadyToReceiveStreamData() == false)
		{
			logte("The stream(%s/%s) is not yet ready to be published.", GetVHostAppName().CStr(), stream->GetName().CStr());
			return false;
		}

		return AddStream(stream);
	}

	bool PushApplication::DeleteAllStreams()
	{
		return Application::DeleteAllStreams();
	}
}