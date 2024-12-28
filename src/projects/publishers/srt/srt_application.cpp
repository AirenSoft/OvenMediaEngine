//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srt_application.h"

#include "srt_private.h"
#include "srt_session.h"
#include "srt_stream.h"

#define logap(format, ...) logtp("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)
#define logad(format, ...) logtd("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)
#define logas(format, ...) logts("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)

#define logai(format, ...) logti("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)
#define logae(format, ...) logte("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)
#define logac(format, ...) logtc("[%s(%u)] " format, GetVHostAppName().CStr(), GetId(), ##__VA_ARGS__)

namespace pub
{
	std::shared_ptr<SrtApplication> SrtApplication::Create(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info)
	{
		auto application = std::make_shared<SrtApplication>(publisher, application_info);

		application->Start();

		return application;
	}

	SrtApplication::SrtApplication(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info)
		: Application(publisher, application_info)
	{
	}

	SrtApplication::~SrtApplication()
	{
		Stop();

		logad("SrtApplication has finally been terminated");
	}

	std::shared_ptr<Stream> SrtApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
	{
		logad("Creating a new SRT stream: %s(%u)", info->GetName().CStr(), info->GetId());

		return SrtStream::Create(GetSharedPtrAs<Application>(), *info, worker_count);
	}

	bool SrtApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
	{
		logad("Deleting the SRT stream: %s(%u)", info->GetName().CStr(), info->GetId());

		auto stream = std::static_pointer_cast<SrtStream>(GetStream(info->GetId()));

		if (stream == nullptr)
		{
			logae("Failed to delete stream - Cannot find stream: %s(%u)", info->GetName().CStr(), info->GetId());
			return false;
		}

		logad("The SRT stream has been deleted: %s(%u)", stream->GetName().CStr(), info->GetId());
		return true;
	}
}  // namespace pub
