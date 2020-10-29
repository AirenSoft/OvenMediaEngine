#pragma once

#include <base/ovsocket/socket_address.h>
#include <base/ovlibrary/ovlibrary.h>

class SignedPolicy
{
public:
	enum class SignedPolicyErrCode : int8_t
	{
		INIT = 0,
		PASSED = 1,
		NO_POLICY_KEY_IN_URL,
		NO_SIGNATURE_KEY_IN_URL,
		NO_SIGNATURE_VALUE_IN_URL,
		INVALID_SIGNATURE,
		INVALID_POLICY,
		BEFORE_POLICY_ACTIATION,
		POLICY_EXPIRED,
		STREAM_EXPIRED,
		UNAUTHORIZED_CLIENT
	};

	// requested_url ==> scheme://domain:port/app/stream[/file]?[query1=value&query2=value&]policy=value&signature=value
	static std::shared_ptr<const SignedPolicy> Load(const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key);

	SignedPolicyErrCode GetState();
	const ov::String& GetLastErrorMessage() const;

	const ov::String& GetRequestedUrl() const;
    const ov::String& GetSecretKey() const;
	const ov::String& GetPolicyQueryKey() const;
	const ov::String& GetPolicyValue() const;
	const ov::String& GetSignatureQueryKey() const;
	const ov::String& GetSignatureValue() const;
	
	// Policy
	uint32_t GetPolicyExpireEpochSec();
	uint32_t GetPolicyActivateEpochSec();
	uint32_t GetStreamExpireEpochSec();
	const ov::String& GetAllowIpCidr() const;

private:
    bool Process(const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key);
	bool ProcessPolicyJson(const ov::String &policy_json);

	bool MakeSignature(const ov::String &base_url, const ov::String &secret_key, ov::String &signature_base64);

	void SetError(SignedPolicyErrCode state, ov::String message)
	{
		_err_code = state;
		_error_message = message;
	}

private:
	SignedPolicyErrCode	_err_code = SignedPolicyErrCode::INIT;
	ov::String _error_message;

	ov::String	_requested_url;
    ov::String  _secret_key;
	ov::String	_policy_query_key;
	ov::String	_policy_text; // Decoded value
	ov::String	_signature_query_key;
	ov::String	_signature_value;

	ov::String  _base_url;
	
	// Policy
	uint64_t	_policy_expire_epoch_msec = 0;
	uint64_t	_policy_activate_epoch_msec = 0;

	uint64_t	_stream_expire_epoch_msec = 0;
	ov::String	_allow_ip_cidr;
};
