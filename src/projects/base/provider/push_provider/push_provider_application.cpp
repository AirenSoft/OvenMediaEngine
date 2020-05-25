//==============================================================================
//
//  PushProviderApplication 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================


#include "push_provider_application.h"
#include "push_provider_private.h"

namespace pvd
{
	PushProviderApplication::PushProviderApplication(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
		: Application(provider, application_info)
	{
	}

	
}