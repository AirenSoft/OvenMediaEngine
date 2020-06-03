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
#include "stream.h"

namespace pvd
{
	class PushProvider;
	class PushApplication : public Application
	{
	public:
		bool JoinStream(const std::shared_ptr<PushStream> &stream);

	protected:
		PushApplication(const std::shared_ptr<PushProvider> &provider, const info::Application &application_info);

		// Deprecated
		virtual std::shared_ptr<pvd::PushStream> CreateStream(const uint32_t stream_id, const ov::String &stream_name){return nullptr;}
	private:

	};
}