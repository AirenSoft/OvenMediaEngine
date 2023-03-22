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

#include "signed_policy/signed_policy.h"
#include "signed_token/signed_token.h"
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

	class RequestInfo
	{
	public:
		RequestInfo(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);
		RequestInfo(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, const ov::String &user_agent);
		RequestInfo(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, const std::shared_ptr<const ov::Url> &new_url);
		RequestInfo(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, const std::shared_ptr<const ov::Url> &new_url, const ov::String &user_agent);

		std::shared_ptr<const ov::Url> GetRequestUrl() const;
		std::shared_ptr<const ov::Url> GetNewUrl() const;
		std::shared_ptr<ov::SocketAddress> GetClientAddress() const;
		const ov::String &GetUserAgent() const;

	private:
		const std::shared_ptr<const ov::Url> _request_url;
		const std::shared_ptr<ov::SocketAddress> _client_address;
		const std::shared_ptr<const ov::Url> _new_url;
		const ov::String _user_agent;
	};

	AccessController(ProviderType provider_type, const cfg::Server &server_config);
	AccessController(PublisherType publisher_type, const cfg::Server &server_config);

	std::tuple<VerificationResult, std::shared_ptr<const SignedPolicy>> VerifyBySignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);

	std::tuple<VerificationResult, std::shared_ptr<const SignedToken>> VerifyBySignedToken(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);

	std::tuple<VerificationResult, std::shared_ptr<const AdmissionWebhooks>> SendCloseWebhooks(const std::shared_ptr<const AccessController::RequestInfo> &request_info);

	std::tuple<VerificationResult, std::shared_ptr<const AdmissionWebhooks>> VerifyByWebhooks(const std::shared_ptr<const AccessController::RequestInfo> &request_info);
	
private:
	const ProviderType _provider_type;
	const PublisherType _publisher_type;
	const cfg::Server _server_config;
};
