//==============================================================================
//
//  PushProvider Application Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/provider/application.h"
#include "push_provider_stream.h"

namespace pvd
{
	class PushProvider;

	class PushProviderApplication : public Application
	{
	public:
		bool AttachStream(std::shared_ptr<PushProviderStream> &stream);
		bool CreateStream(const ov::String stream_name, const std::shared_ptr<ov::Socket> &remote);

	protected:
		PushProviderApplication(const std::shared_ptr<Provider> &provider, const info::Application &application_info);

	private:

	};
}