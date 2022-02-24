#include "access_controller.h"

#include <orchestrator/orchestrator.h>

#define OV_LOG_TAG "AccessController"

AccessController::AccessController(ProviderType provider_type, const cfg::Server &server_config)
	: _provider_type(provider_type), _publisher_type(PublisherType::Unknown), _server_config(server_config)
{

}

AccessController::AccessController(PublisherType publisher_type, const cfg::Server &server_config)
	: _provider_type(ProviderType::Unknown), _publisher_type(publisher_type), _server_config(server_config)
{

}


std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> AccessController::SendCloseWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
{
	auto orchestrator = ocst::Orchestrator::GetInstance();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

	if (vhost_name.IsEmpty())
	{
		logte("Could not resolve the domain: %s", request_url->Host().CStr());
		return {AccessController::VerificationResult::Error, nullptr};
	}

	auto item = ocst::Orchestrator::GetInstance()->GetHostInfo(vhost_name);
	if(item.has_value())
	{
		auto vhost_item = item.value();

		auto &webhooks_config = vhost_item.GetAdmissionWebhooks();
		if (!webhooks_config.IsParsed())
		{
			// The vhost doesn't use the AdmissionWebhooks feature.
			return {AccessController::VerificationResult::Off, nullptr};
		}

		if(_provider_type != ProviderType::Unknown)
		{
			if(webhooks_config.IsEnabledProvider(_provider_type) == false)
			{
				// This provider turned off the AdmissionWebhooks function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else if(_publisher_type != PublisherType::Unknown)
		{
			if(webhooks_config.IsEnabledPublisher(_publisher_type) == false)
			{
				// This publisher turned off the AdmissionWebhooks function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else
		{
			logte("Could not resolve provider/publisher type: %s", request_url->Host().CStr());
			return {AccessController::VerificationResult::Error, nullptr};
		}

		auto control_server_url_address = webhooks_config.GetControlServerUrl();
		auto control_server_url = ov::Url::Parse(control_server_url_address);
		auto secret_key = webhooks_config.GetSecretKey();
		auto timeout_msec = 500; //webhooks_config.GetTimeoutMsec();

		std::shared_ptr<AdmissionWebhooks> admission_webhooks;
		if(_provider_type != ProviderType::Unknown)
		{
			admission_webhooks = AdmissionWebhooks::Query(
				_provider_type, control_server_url, timeout_msec, secret_key, client_address, request_url, AdmissionWebhooks::Status::Code::CLOSING);
		}
		else if(_publisher_type != PublisherType::Unknown)
		{
			admission_webhooks = AdmissionWebhooks::Query(
				_publisher_type, control_server_url, timeout_msec, secret_key, client_address, request_url, AdmissionWebhooks::Status::Code::CLOSING);
		}
		else
		{
			logte("Provider type or publisher type must be set");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		if(admission_webhooks == nullptr)
		{
			// Probably this doesn't happen
			logte("Could not load admission webhooks");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		logti("AdmissionWebhooks queried %s whether client %s could access %s. (Result : %s Elapsed : %u ms)",
			control_server_url_address.CStr(), client_address->ToString(false).CStr(), request_url->ToUrlString().CStr(),
			admission_webhooks->GetErrCode()==AdmissionWebhooks::ErrCode::ALLOWED?"Allow":"Reject", admission_webhooks->GetElpasedTime());

		if(admission_webhooks->GetErrCode() != AdmissionWebhooks::ErrCode::ALLOWED)
		{
			return {AccessController::VerificationResult::Fail, admission_webhooks};
		}

		return {AccessController::VerificationResult::Pass, admission_webhooks};
	}

	// Probably this doesn't happen
	logte("Could not find VirtualHost (%s)", vhost_name);
	return {AccessController::VerificationResult::Error, nullptr};
}

std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> AccessController::VerifyByWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
{
	auto orchestrator = ocst::Orchestrator::GetInstance();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

	if (vhost_name.IsEmpty())
	{
		logte("Could not resolve the domain: %s", request_url->Host().CStr());
		return {AccessController::VerificationResult::Error, nullptr};
	}

	auto item = ocst::Orchestrator::GetInstance()->GetHostInfo(vhost_name);
	if(item.has_value())
	{
		auto vhost_item = item.value();

		auto &webhooks_config = vhost_item.GetAdmissionWebhooks();
		if (!webhooks_config.IsParsed())
		{
			// The vhost doesn't use the SignedPolicy  feature.
			return {AccessController::VerificationResult::Off, nullptr};
		}

		if(_provider_type != ProviderType::Unknown)
		{
			if(webhooks_config.IsEnabledProvider(_provider_type) == false)
			{
				// This provider turned off the SignedPolicy function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else if(_publisher_type != PublisherType::Unknown)
		{
			if(webhooks_config.IsEnabledPublisher(_publisher_type) == false)
			{
				// This publisher turned off the SignedPolicy function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else
		{
			logte("Could not resolve provider/publisher type: %s", request_url->Host().CStr());
			return {AccessController::VerificationResult::Error, nullptr};
		}

		auto control_server_url_address = webhooks_config.GetControlServerUrl();
		auto control_server_url = ov::Url::Parse(control_server_url_address);
		auto secret_key = webhooks_config.GetSecretKey();
		auto timeout_msec = webhooks_config.GetTimeoutMsec();

		std::shared_ptr<AdmissionWebhooks> admission_webhooks;
		if(_provider_type != ProviderType::Unknown)
		{
			admission_webhooks = AdmissionWebhooks::Query(_provider_type, control_server_url, timeout_msec, secret_key, client_address, request_url);
		}
		else if(_publisher_type != PublisherType::Unknown)
		{
			admission_webhooks = AdmissionWebhooks::Query(_publisher_type, control_server_url, timeout_msec, secret_key, client_address, request_url);
		}
		else
		{
			logte("Provider type or publisher type must be set");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		if(admission_webhooks == nullptr)
		{
			// Probably this doesn't happen
			logte("Could not load admission webhooks");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		logti("AdmissionWebhooks queried %s whether client %s could access %s. (Result : %s Elapsed : %u ms)",
			control_server_url_address.CStr(), client_address->ToString(false).CStr(), request_url->ToUrlString().CStr(), admission_webhooks->GetErrCode()==AdmissionWebhooks::ErrCode::ALLOWED?"Allow":"Reject", admission_webhooks->GetElpasedTime());

		if(admission_webhooks->GetErrCode() != AdmissionWebhooks::ErrCode::ALLOWED)
		{
			return {AccessController::VerificationResult::Fail, admission_webhooks};
		}

		return {AccessController::VerificationResult::Pass, admission_webhooks};
	}

	// Probably this doesn't happen
	logte("Could not find VirtualHost (%s)", vhost_name);
	return {AccessController::VerificationResult::Error, nullptr};
}

std::tuple<AccessController::VerificationResult, std::shared_ptr<const SignedPolicy>> AccessController::VerifyBySignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
{
	auto orchestrator = ocst::Orchestrator::GetInstance();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

	if (vhost_name.IsEmpty())
	{
		logte("Could not resolve the domain: %s", request_url->Host().CStr());
		return {AccessController::VerificationResult::Error, nullptr};
	}

	auto item = ocst::Orchestrator::GetInstance()->GetHostInfo(vhost_name);
	if(item.has_value())
	{
		auto vhost_item = item.value();
		// Handle SignedPolicy if needed
		auto &signed_policy_config = vhost_item.GetSignedPolicy();
		if (!signed_policy_config.IsParsed())
		{
			// The vhost doesn't use the SignedPolicy  feature.
			return {AccessController::VerificationResult::Off, nullptr};
		}

		if(_provider_type != ProviderType::Unknown)
		{
			if(signed_policy_config.IsEnabledProvider(_provider_type) == false)
			{
				// This provider turned off the SignedPolicy function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else if(_publisher_type != PublisherType::Unknown)
		{
			if(signed_policy_config.IsEnabledPublisher(_publisher_type) == false)
			{
				// This publisher turned off the SignedPolicy function
				return {AccessController::VerificationResult::Off, nullptr};
			}
		}
		else
		{
			logte("Could not resolve provider/publisher type: %s", request_url->Host().CStr());
			return {AccessController::VerificationResult::Error, nullptr};
		}

		auto policy_query_key_name = signed_policy_config.GetPolicyQueryKeyName();
		auto signature_query_key_name = signed_policy_config.GetSignatureQueryKeyName();
		auto secret_key = signed_policy_config.GetSecretKey();

		auto signed_policy = SignedPolicy::Load(client_address->GetIpAddress(), request_url->ToUrlString(), policy_query_key_name, signature_query_key_name, secret_key);
		if(signed_policy == nullptr)
		{
			// Probably this doesn't happen
			logte("Could not load SingedPolicy");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		if(signed_policy->GetErrCode() != SignedPolicy::ErrCode::PASSED)
		{
			return {AccessController::VerificationResult::Fail, signed_policy};
		}

		return {AccessController::VerificationResult::Pass, signed_policy};
	}

	// Probably this doesn't happen
	logte("Could not find VirtualHost (%s)", vhost_name.CStr());
	return {AccessController::VerificationResult::Error, nullptr};
}

std::tuple<AccessController::VerificationResult, std::shared_ptr<const SignedToken>> AccessController::VerifyBySignedToken(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
{
	auto orchestrator = ocst::Orchestrator::GetInstance();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

	if (vhost_name.IsEmpty())
	{
		logte("Could not resolve the domain: %s", request_url->Host().CStr());
		return {AccessController::VerificationResult::Error, nullptr};
	}

	auto item = ocst::Orchestrator::GetInstance()->GetHostInfo(vhost_name);
	if(item.has_value())
	{
		auto vhost_item = item.value();

		// Handle SignedToken if needed
		auto &signed_token_config = vhost_item.GetSignedToken();
		if (!signed_token_config.IsParsed() || signed_token_config.GetCryptoKey().IsEmpty())
		{
			// The vhost doesn't use the signed url feature.
			return {AccessController::VerificationResult::Off, nullptr};
		}

		auto crypto_key = signed_token_config.GetCryptoKey();
		auto query_string_key = signed_token_config.GetQueryStringKey();

		auto signed_token = SignedToken::Load(client_address->ToString(), request_url->ToUrlString(), query_string_key, crypto_key);
		if (signed_token == nullptr)
		{
			// Probably this doesn't happen
			logte("Could not load SingedToken");
			return {AccessController::VerificationResult::Error, nullptr};
		}

		if(signed_token->GetErrCode() != SignedToken::ErrCode::PASSED)
		{
			return {AccessController::VerificationResult::Fail, signed_token};
		}

		return {AccessController::VerificationResult::Pass, signed_token};
	}

	// Probably this doesn't happen
	logte("Could not find VirtualHost (%s)", vhost_name.CStr());
	return {AccessController::VerificationResult::Error, nullptr}; 
}
