//==============================================================================
//
//  Authenticate Base Class
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/info/host.h>

#include "request_info.h"
#include "signed_policy/signed_policy.h"
#include "admission_webhooks/admission_webhooks.h"

class AccessController
{
public:
	enum class VerificationResult
	{
		Error = -2,		// Unexpected error
		Fail = -1,		// Verified but fail
		Off = 0,		// configuration is off
		Pass = 1		// Check signature and pass
	};

	AccessController(ProviderType provider_type, const cfg::Server &server_config);
	AccessController(PublisherType publisher_type, const cfg::Server &server_config);

	std::tuple<VerificationResult, std::shared_ptr<const SignedPolicy>> VerifyBySignedPolicy(const std::shared_ptr<const ac::RequestInfo> &request_info);

	std::tuple<VerificationResult, std::shared_ptr<const AdmissionWebhooks>> VerifyByWebhooks(const std::shared_ptr<const ac::RequestInfo> &request_info);

	std::tuple<VerificationResult, std::shared_ptr<const AdmissionWebhooks>> SendCloseWebhooks(const std::shared_ptr<const ac::RequestInfo> &request_info);

private:
	const ProviderType _provider_type;
	const PublisherType _publisher_type;
	const cfg::Server _server_config;
};
