//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <openssl/srtp.h>
#include "srtp_adapter.h"

SrtpAdapter::SrtpAdapter()
{
	_session = nullptr;
	_rtp_auth_tag_len = 0;
	_rtcp_auth_tag_len = 0;
}

SrtpAdapter::~SrtpAdapter()
{
}

bool SrtpAdapter::SetKey(srtp_ssrc_type_t type, uint64_t crypto_suite, std::shared_ptr<ov::Data> key)
{
	srtp_policy_t policy;
	memset(&policy, 0, sizeof(policy));

	switch(crypto_suite)
	{
		case SRTP_AES128_CM_SHA1_80:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
			break;
		case SRTP_AES128_CM_SHA1_32:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
			break;
		default:
			loge("SRTP", "Failed to create srtp adapter. Unsupported crypto suite %d", crypto_suite);
			return false;
	}

	policy.ssrc.type = type;
	policy.ssrc.value = 0;
	policy.key = key->GetWritableDataAs<uint8_t>();
	policy.window_size = 1024;
	policy.allow_repeat_tx = 1;
	policy.next = nullptr;

	int err = srtp_create(&_session, &policy);
	if (err != srtp_err_status_ok)
	{
		_session = nullptr;
		loge("SRTP", "srtp_create failed, err=%d", err);
		return false;
	}
	srtp_set_user_data(_session, this);

	_rtp_auth_tag_len = policy.rtp.auth_tag_len;
	_rtcp_auth_tag_len = policy.rtcp.auth_tag_len;

	return true;
}

bool SrtpAdapter::ProtectRtp(std::shared_ptr<ov::Data> data)
{
	if(!_session)
	{
		return false;
	}

	// Protect를 하면 다음과 같은 사이즈가 필요하다. data의 Capacity가 충분해야 한다.
	int need_len = static_cast<int>(data->GetLength()) + _rtp_auth_tag_len;

	if(need_len > data->GetCapacity())
	{
		loge("SRTP", "Buffer capacity(%d) less than the needed(%d)", data->GetCapacity(), need_len);
		return false;
	}

	auto buffer = data->GetWritableData();
	int out_len = static_cast<int>(data->GetLength());
 	data->SetLength(need_len);
	int err = srtp_protect(_session, buffer, &out_len);
	if (err != srtp_err_status_ok)
	{
		loge("SRTP", "Failed to protect SRTP packet, err=%d", err);
		return false;
	}

	return true;
}