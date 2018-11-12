//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/config.h>

#include "stream.h"

namespace info
{
	typedef uint32_t application_id_t;

	class Application : public cfg::Application
	{
	public:
		explicit Application(const cfg::Application &application)
			: cfg::Application(application)
		{
			_application_id = ov::Random::GenerateInteger();
		}

		application_id_t GetId() const
		{
			return _application_id;
		}

	protected:
		application_id_t _application_id = 0;
	};
}