#pragma once

#include <base/ovsocket/socket_address.h>
#include <base/ovlibrary/ovlibrary.h>

class SignedPolicy
{
public:
	enum class ErrCode : int8_t
	{
		INIT = 0,
		PASSED = 1,
		NO_POLICY_KEY_IN_URL,
		NO_SIGNATURE_KEY_IN_URL,
		NO_SIGNATURE_VALUE_IN_URL,
		INVALID_SIGNATURE,
		INVALID_POLICY,
		BEFORE_URL_ACTIATION,
		URL_EXPIRED,
		STREAM_EXPIRED,
		UNAUTHORIZED_CLIENT
	};

	// requested_url ==> scheme://domain:port/app/stream[/file]?[query1=value&query2=value&]policy=value&signature=value
	static std::shared_ptr<const SignedPolicy> Load(const ov::String &client_address, const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key);

	ErrCode GetErrCode() const
	{
		return _error_code;
	}

	const ov::String& GetErrMessage() const
	{
		return _error_message;
	}

	const ov::String& GetRequestedUrl() const;
    const ov::String& GetSecretKey() const;
	const ov::String& GetPolicyQueryKeyName() const;
	const ov::String& GetPolicyValue() const;
	const ov::String& GetSignatureQueryKeyName() const;
	const ov::String& GetSignatureValue() const;
	
	// Policy
	uint64_t GetPolicyExpireEpochSec() const;
	uint64_t GetPolicyActivateEpochSec() const;
	uint64_t GetStreamExpireEpochSec() const;
	const ov::String& GetAllowIpCidr() const;

private:
    bool Process(const ov::String &client_address, const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key);
	bool ProcessPolicyJson(const ov::String &policy_json);

	bool MakeSignature(const ov::String &base_url, const ov::String &secret_key, ov::String &signature_base64);

	void SetError(ErrCode state, ov::String message)
	{
		_error_code = state;
		_error_message = message;
	}

private:
	ErrCode	_error_code = ErrCode::INIT;
	ov::String _error_message;

	ov::String	_requested_url;
    ov::String  _secret_key;
	ov::String	_policy_query_key_name;
	ov::String	_policy_text; // Decoded value
	ov::String	_signature_query_key_name;
	ov::String	_signature_value;

	ov::String  _base_url;
	
	// Policy
	uint64_t	_url_expire_epoch_msec = 0;
	uint64_t	_url_activate_epoch_msec = 0;

	uint64_t	_stream_expire_epoch_msec = 0;
	ov::String	_allow_ip_cidr;
};
