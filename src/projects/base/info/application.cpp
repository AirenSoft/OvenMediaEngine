//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"
#include "application_private.h"

namespace info
{
	Application::Application(const cfg::Application &application)
			: cfg::Application(application)
	{
		_application_id = ov::Random::GenerateUInt32();
	}
}