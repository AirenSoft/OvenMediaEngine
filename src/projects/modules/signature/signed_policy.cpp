#include <openssl/evp.h>
#include <openssl/des.h>
#include <base/ovcrypto/base_64.h>
#include <base/ovlibrary/converter.h>
#include <base/ovcrypto/message_digest.h>

#include "signed_policy.h"

// requested_url ==> scheme://domain:port/app/stream[/file]?[query1=value&query2=value&]policy=value&signature=value
std::shared_ptr<const SignedPolicy> SignedPolicy::Load(const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key)
{
	auto signed_policy = std::make_shared<SignedPolicy>();
	signed_policy->Process(requested_url, policy_query_key, signature_query_key, secret_key);
	return signed_policy;
}

bool SignedPolicy::Process(const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key)
{
	auto url = ov::Url::Parse(requested_url);
	
	if(url->HasQueryKey(signature_query_key) == false)
	{
		SetError(SignedPolicyErrCode::NO_SIGNATURE_KEY_IN_URL, ov::String::FormatString("There is no sinature key(%s) in url(%s).", signature_query_key.CStr(), requested_url.CStr()));
		return false;
	}

	
	if(url->HasQueryKey(policy_query_key) == false)
	{
		SetError(SignedPolicyErrCode::NO_POLICY_KEY_IN_URL, ov::String::FormatString("There is no policy key(%s) in url(%s).", policy_query_key.CStr(), requested_url.CStr()));
		return false;
	}

	// Extract signature
	auto signature_query_value = url->GetQueryValue(signature_query_key);
	if(signature_query_value.IsEmpty())
	{
		SetError(SignedPolicyErrCode::NO_SIGNATURE_VALUE_IN_URL, ov::String::FormatString("Signature value is empty in url(%s).", requested_url.CStr()));
		return false;
	}

	// Separate the signature to make base url of signature
	url->RemoveQueryKey(signature_query_key);
	auto base_url = url->ToUrlString(true);

	// Make signature
	ov::String signature_base64;
	if(MakeSignature(base_url, secret_key, signature_base64) == false)
	{
		SetError(SignedPolicyErrCode::NO_SIGNATURE_VALUE_IN_URL, ov::String::FormatString("Could not generate signature from url(%s).", requested_url.CStr()));
		return false;
	}

	if(signature_base64 != signature_query_value)
	{
		SetError(SignedPolicyErrCode::INVALID_SIGNATURE, ov::String::FormatString("Signature value is invalid(expected : %s | input : %s).", signature_base64.CStr(), signature_query_value.CStr()));
		return false;
	}

	// Extract policy
	auto policy_base64 = url->GetQueryValue(policy_query_key);
	auto policy = ov::Base64::Decode(policy_base64);

	if(ProcessPolicyJson(policy->ToString()) == false)
	{
		return false;
	}
	
	return true;
}

bool SignedPolicy::MakeSignature(const ov::String &base_url, const ov::String &secret_key, ov::String &signature_base64)
{
	auto md = ov::MessageDigest::ComputeHmac(ov::CryptoAlgorithm::Sha1, secret_key.ToData(), base_url.ToData());	
	if(md == nullptr)
	{
		return false;
	}

	signature_base64 = ov::Base64::Encode(md);

	return true;
}

/*	Policy format
{
	"activate":1399721576,									
	"policy_expire":1399721581,									
	"stream_expire":1399731576,									
	"allow_ip": "192.168.100.5/32"
}
*/ 
bool SignedPolicy::ProcessPolicyJson(const ov::String &policy_json)
{
	ov::JsonObject object = ov::Json::Parse(policy_json);
	if(object.IsNull())
	{
		SetError(SignedPolicyErrCode::INVALID_POLICY, ov::String::FormatString("The policy is in worng format.", policy_json.CStr()));
		return false;
	}

	Json::Value &jv_activate = object.GetJsonValue()["activate"];
	Json::Value &jv_policy_expire = object.GetJsonValue()["policy_expire"];
	Json::Value &jv_stream_expire = object.GetJsonValue()["stream_expire"];
	Json::Value &jv_allow_ip = object.GetJsonValue()["allow_ip"];

	if(jv_policy_expire.isNull() || !jv_policy_expire.isUInt())
	{
		SetError(SignedPolicyErrCode::INVALID_POLICY, ov::String::FormatString("policy_expire must be uint32_t and is a required value.", policy_json.CStr()));
		return false;
	}
	else
	{
		_policy_expire_epoch_msec = jv_policy_expire.asUInt();

		// Policy expired
		if(_policy_expire_epoch_msec < ov::Clock::NowMSec())
		{
			SetError(SignedPolicyErrCode::INVALID_POLICY, ov::String::FormatString("Policy has expired.(now:%u policy_expire:%u) ", ov::Clock::NowMSec(), _policy_expire_epoch_msec));
			return false;
		}
	}
	
	if(!jv_activate.isNull() && jv_activate.isUInt())
	{
		_policy_activate_epoch_msec = jv_activate.asUInt();

		// Policy is not activated yet
		if(_policy_activate_epoch_msec > ov::Clock::NowMSec())
		{
			SetError(SignedPolicyErrCode::INVALID_POLICY, ov::String::FormatString("The policy has not yet been activated.(now:%u policy_activate:%u) ", ov::Clock::NowMSec(), _policy_activate_epoch_msec));
			return false;
		}
	}

	if(!jv_stream_expire.isNull() && jv_stream_expire.isUInt())
	{
		_stream_expire_epoch_msec = jv_stream_expire.asUInt();
	}
	
	if(!jv_allow_ip.isNull() && jv_allow_ip.isString())
	{
		_allow_ip_cidr = jv_stream_expire.asCString();
	}

	return true;
}

SignedPolicy::SignedPolicyErrCode SignedPolicy::GetState()
{
	return _err_code;
}

const ov::String& SignedPolicy::GetLastErrorMessage() const 
{
	return _error_message;
}

const ov::String& SignedPolicy::GetRequestedUrl() const
{
	return _requested_url;
}

const ov::String& SignedPolicy::GetSecretKey() const
{
	return _secret_key;
}

const ov::String& SignedPolicy::GetPolicyQueryKey() const
{
	return _policy_query_key;
}

const ov::String& SignedPolicy::GetPolicyValue() const
{
	return _policy_text;
}

const ov::String& SignedPolicy::GetSignatureQueryKey() const
{
	return _signature_query_key;
}

const ov::String& SignedPolicy::GetSignatureValue() const
{
	return _signature_value;
}


// Policy
uint32_t SignedPolicy::GetPolicyExpireEpochSec()
{
	return _policy_expire_epoch_msec;
}

uint32_t SignedPolicy::GetPolicyActivateEpochSec()
{
	return _policy_activate_epoch_msec;
}

uint32_t SignedPolicy::GetStreamExpireEpochSec()
{
	return _stream_expire_epoch_msec;
}

const ov::String& SignedPolicy::GetAllowIpCidr() const
{
	return _allow_ip_cidr;
}
