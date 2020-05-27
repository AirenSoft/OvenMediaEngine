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
		std::shared_ptr<pvd::PushStream> CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks);
		
	protected:
		PushApplication(const std::shared_ptr<PushProvider> &provider, const info::Application &application_info);

		virtual std::shared_ptr<pvd::PushStream> CreateStream(const uint32_t stream_id, const ov::String &stream_name) = 0;

	private:

	};
}