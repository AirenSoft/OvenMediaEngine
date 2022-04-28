//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "llhls_application.h"
#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsApplication> LLHlsApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<LLHlsApplication>(publisher, application_info);
	application->Start();
	return application;
}

LLHlsApplication::LLHlsApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{

}

LLHlsApplication::~LLHlsApplication()
{
	Stop();
	logtd("LLHlsApplication(%d) has been terminated finally", GetId());
}

bool LLHlsApplication::Start()
{
	return Application::Start();
}

bool LLHlsApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<pub::Stream> LLHlsApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("Created stream : %s/%u", info->GetName().CStr(), info->GetId());

	return LLHlsStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool LLHlsApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("LLHlsApplication::DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<LLHlsStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("LLHlsApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	logtd("LLHlsApplication %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}
