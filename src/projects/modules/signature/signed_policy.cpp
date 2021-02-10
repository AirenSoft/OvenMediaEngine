#include <openssl/evp.h>
#include <base/ovcrypto/base_64.h>
#include <base/ovlibrary/converter.h>
#include <base/ovcrypto/message_digest.h>

#include "signed_policy.h"


// requested_url ==> scheme://domain:port/app/stream[/file]?[query1=value&query2=value&]policy=value&signature=value
std::shared_ptr<const SignedPolicy> SignedPolicy::Load(const ov::String &client_address, const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key)
{
	auto signed_policy = std::make_shared<SignedPolicy>();
	signed_policy->Process(client_address, requested_url, policy_query_key, signature_query_key, secret_key);
	return signed_policy;
}

bool SignedPolicy::Process(const ov::String &client_address, const ov::String &requested_url, const ov::String &policy_query_key, const ov::String &signature_query_key, const ov::String &secret_key)
{
	auto url = ov::Url::Parse(requested_url);
	
	if(url->HasQueryKey(signature_query_key) == false)
	{
		SetError(ErrCode::NO_SIGNATURE_KEY_IN_URL, ov::String::FormatString("There is no sinature key(%s) in url(%s).", signature_query_key.CStr(), requested_url.CStr()));
		return false;
	}

	
	if(url->HasQueryKey(policy_query_key) == false)
	{
		SetError(ErrCode::NO_POLICY_KEY_IN_URL, ov::String::FormatString("There is no policy key(%s) in url(%s).", policy_query_key.CStr(), requested_url.CStr()));
		return false;
	}

	// Extract signature
	auto signature_query_value = url->GetQueryValue(signature_query_key);
	if(signature_query_value.IsEmpty())
	{
		SetError(ErrCode::NO_SIGNATURE_VALUE_IN_URL, ov::String::FormatString("Signature value is empty in url(%s).", requested_url.CStr()));
		return false;
	}

	// Separate the signature to make base url of signature
	url->RemoveQueryKey(signature_query_key);
	auto base_url = url->ToUrlString(true);

	// Make signature
	ov::String signature_base64;
	if(MakeSignature(base_url, secret_key, signature_base64) == false)
	{
		SetError(ErrCode::NO_SIGNATURE_VALUE_IN_URL, ov::String::FormatString("Could not generate signature from url(%s).", requested_url.CStr()));
		return false;
	}

	if(signature_base64 != signature_query_value)
	{
		SetError(ErrCode::INVALID_SIGNATURE, ov::String::FormatString("Signature value is invalid(expected : %s | input : %s).", signature_base64.CStr(), signature_query_value.CStr()));
		return false;
	}

	// Extract policy
	auto policy_base64 = url->GetQueryValue(policy_query_key);
	auto policy = ov::Base64::Decode(policy_base64, true);

	if(ProcessPolicyJson(policy->ToString()) == false)
	{
		return false;
	}

	// Check IP
	if(IsAllowedIP(client_address) == false)
	{	
		SetError(ErrCode::UNAUTHORIZED_CLIENT, ov::String::FormatString("%s IP address is not allowed.(Allowed range : %s ~ %s)", client_address.CStr(), _cidr->Begin().CStr(), _cidr->End().CStr()));
		return false;
	}

	SetError(ErrCode::PASSED, "Authorized");
	
	return true;
}

bool SignedPolicy::MakeSignature(const ov::String &base_url, const ov::String &secret_key, ov::String &signature_base64)
{
	auto md = ov::MessageDigest::ComputeHmac(ov::CryptoAlgorithm::Sha1, secret_key.ToData(false), base_url.ToData(false));
	if(md == nullptr)
	{
		return false;
	}

	signature_base64 = ov::Base64::Encode(md, true);

	return true;
}

/*	Policy format
{
	"url_activate":1399721576,									
	"url_expire":1399721581,									
	"stream_expire":1399731576,									
	"allow_ip": "192.168.100.5/32"
}
*/ 
bool SignedPolicy::ProcessPolicyJson(const ov::String &policy_json)
{
	ov::JsonObject object = ov::Json::Parse(policy_json);
	if(object.IsNull())
	{
		SetError(ErrCode::INVALID_POLICY, ov::String::FormatString("The policy is in wrong format.", policy_json.CStr()));
		return false;
	}

	Json::Value &jv_url_activate = object.GetJsonValue()["url_activate"];
	Json::Value &jv_url_expire = object.GetJsonValue()["url_expire"];
	Json::Value &jv_stream_expire = object.GetJsonValue()["stream_expire"];
	Json::Value &jv_allow_ip = object.GetJsonValue()["allow_ip"];

	if(jv_url_expire.isNull() || !jv_url_expire.isUInt64())
	{
		SetError(ErrCode::INVALID_POLICY, ov::String::FormatString("url_expire must be epoch milliseconds as uint64_t and is a required value.", policy_json.CStr()));
		return false;
	}
	else
	{
		_url_expire_epoch_msec = jv_url_expire.asUInt64();

		// Policy expired
		if(_url_expire_epoch_msec < ov::Clock::NowMSec())
		{
			SetError(ErrCode::INVALID_POLICY, ov::String::FormatString("URL has expired.(now:%llu policy_expire:%llu) ", ov::Clock::NowMSec(), _url_expire_epoch_msec));
			return false;
		}
	}
	
	if(!jv_url_activate.isNull() && jv_url_activate.isUInt64())
	{
		_url_activate_epoch_msec = jv_url_activate.asUInt64();

		// Policy is not activated yet
		if(_url_activate_epoch_msec > ov::Clock::NowMSec())
		{
			SetError(ErrCode::INVALID_POLICY, ov::String::FormatString("The URL has not yet been activated.(now:%llu policy_activate:%llu) ", ov::Clock::NowMSec(), _url_activate_epoch_msec));
			return false;
		}
	}

	if(!jv_stream_expire.isNull() && jv_stream_expire.isUInt64())
	{
		_stream_expire_epoch_msec = jv_stream_expire.asUInt64();
	}
	
	if(!jv_allow_ip.isNull() && jv_allow_ip.isString())
	{
		_allow_ip_cidr = jv_allow_ip.asString().c_str();
		_cidr = ov::CIDR::Parse(_allow_ip_cidr);
		if(_cidr == nullptr)
		{
			SetError(ErrCode::INVALID_POLICY, ov::String::FormatString("allow_ip:%s in SignedPolicy is an invalid CIDR.", _allow_ip_cidr.CStr()));
			return false;
		}
	}

	return true;
}

const ov::String& SignedPolicy::GetRequestedUrl() const
{
	return _requested_url;
}

const ov::String& SignedPolicy::GetSecretKey() const
{
	return _secret_key;
}

const ov::String& SignedPolicy::GetPolicyQueryKeyName() const
{
	return _policy_query_key_name;
}

const ov::String& SignedPolicy::GetPolicyValue() const
{
	return _policy_text;
}

const ov::String& SignedPolicy::GetSignatureQueryKeyName() const
{
	return _signature_query_key_name;
}

const ov::String& SignedPolicy::GetSignatureValue() const
{
	return _signature_value;
}


// Policy
uint64_t SignedPolicy::GetPolicyExpireEpochSec() const
{
	return _url_expire_epoch_msec;
}

uint64_t SignedPolicy::GetPolicyActivateEpochSec() const
{
	return _url_activate_epoch_msec;
}

uint64_t SignedPolicy::GetStreamExpireEpochSec() const
{
	return _stream_expire_epoch_msec;
}

const ov::String& SignedPolicy::GetAllowIpCidr() const
{
	return _allow_ip_cidr;
}

bool SignedPolicy::IsAllowedIP(const ov::String &ip_addr) const
{
	// Do not have IP policy
	if(_cidr == nullptr)
	{
		return true;
	}

	return _cidr->CheckIP(ip_addr);
}

bool SignedPolicy::GetCIDRRange(ov::String &begin, ov::String &end) const
{
	if(_cidr == nullptr)
	{
		return false;
	}
	begin = _cidr->Begin();
	end = _cidr->End();
	return true;
}