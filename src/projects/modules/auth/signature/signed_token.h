#pragma once

#include <base/ovsocket/socket_address.h>
#include <base/ovlibrary/ovlibrary.h>

class SignedToken
{
public:
	enum class ErrCode : int8_t
	{
		INIT = 0,
		PASSED = 1,
		NO_TOKEN_KEY_IN_URL,
		NO_TOKEN_VALUE_IN_URL,
		DECRYPT_FAILED,
		INVALID_TOKEN_FORMAT,
		TOKEN_EXPIRED,
		STREAM_EXPIRED,
		UNAUTHORIZED_CLIENT,
		INVALID_URL
	};

	static std::shared_ptr<const SignedToken> Load(const ov::String &client_address, const ov::String &request_url, const ov::String &token_query_key, const ov::String &secret_key);

	ErrCode GetErrCode() const
	{
		return _error_code;
	}

	const ov::String& GetErrMessage() const
	{
		return _error_message;
	}

    const ov::String&       GetUrl() const;
    const ov::String&       GetClientIP() const;
    const ov::String&       GetSessionID() const;
    uint64_t                GetTokenExpiredTime() const;
    uint64_t                GetStreamExpiredTime() const;
    bool                    IsAllowedClient(const ov::String &address) const;
    bool                    IsTokenExpired() const;
    bool                    IsStreamExpired() const;

private:
    bool Process(const ov::String &client_address, const ov::String &request_url, const ov::String &token_query_key, const ov::String &secret_key);
    bool Encrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &plain_in, ov::Data &encrypted_out) const;
    bool Decrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &encrypted_in, ov::Data &plain_out) const;
    
	void SetError(ErrCode code, ov::String message)
	{
		_error_code = code;
		_error_message = message;
	}

private:
	ErrCode	_error_code = ErrCode::INIT;
	ov::String _error_message;

    ov::String  _key;
    ov::String  _full_string;

    ov::String  _url;
    ov::String  _client_ip;
    ov::String  _session_id;
    uint64_t    _token_expired_time;
    uint64_t    _stream_expired_time;
};
