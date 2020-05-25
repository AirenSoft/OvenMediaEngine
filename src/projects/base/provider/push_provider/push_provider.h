//==============================================================================
//
//  PushProvider Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/provider/provider.h>
#include <base/provider/stream_motor.h>

#include "push_provider_application.h"
#include "push_provider_stream.h"

namespace pvd
{
    class PushProvider : public Provider
    {
    public:
        virtual bool Start();
		virtual bool Stop();

		bool AssignStream(std::shared_ptr<PushProviderStream> &stream, ov::String app_name);

    protected:
		PushProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		virtual ~PushProvider();

    private:
		
    };
}