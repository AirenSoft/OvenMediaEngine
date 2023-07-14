//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srtpush_application.h"

#include "srtpush_private.h"


std::shared_ptr<SrtPushApplication> SrtPushApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<SrtPushApplication>(publisher, application_info);
	application->Start();
	return application;
}

SrtPushApplication::SrtPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: MpegtsPushApplication(publisher, application_info)
{
}

SrtPushApplication::~SrtPushApplication()
{
}

std::shared_ptr<ov::Error> SrtPushApplication::StartPush(const std::shared_ptr<info::Push> &push)
{
	// Validation check for required parameters
	if (push->GetId().IsEmpty() == true ||
		push->GetStreamName().IsEmpty() == true ||
		push->GetUrl().IsEmpty() == true ||
		push->GetProtocol().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (push->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		if (push->GetStreamName().IsEmpty() == true)
		{
			error_message += " stream.name";
		}

		if (push->GetUrl().IsEmpty() == true)
		{
			error_message += " url";
		}

		if (push->GetProtocol().IsEmpty() == true)
		{
			error_message += " protocol";
		}

		error_message += "]";
		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Validation check for protocol scheme
	if (push->GetUrl().HasPrefix("srt://") == false)
	{
		ov::String error_message = "Unsupported protocol";

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Remove suffix '/" of srt url
	while (push->GetUrl().HasSuffix("/"))
	{
		ov::String tmp_url = push->GetUrl().Substring(0, push->GetUrl().IndexOfRev('/'));
		push->SetUrl(tmp_url);
	}

	// Validation check for srt's connection mode.
	auto srt_url = ov::Url::Parse(push->GetUrl());
	if (srt_url == nullptr)
	{
		logte("Could not parse url: %s", push->GetUrl().CStr());
		ov::String error_message = "Could not parse url";
		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	auto connection_mode = srt_url->GetQueryValue("mode");
	if (!connection_mode.IsEmpty() &&
		connection_mode != "caller")
	{
		ov::String error_message = "Unsupported connection mode";
		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	return PushApplication::StartPush(push);
}