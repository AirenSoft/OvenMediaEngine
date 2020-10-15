#pragma once

#include <base/ovsocket/socket_address.h>
#include <base/ovlibrary/ovlibrary.h>

enum class SignedUrlType
{
    // Type0 : Des.Decrypt(Base64.Decode("[url],[client ip],[token expired time],[stream expired time]", key, ECB Mode, Pkcs5 Padding))
    Type0, 
    // No defined yet
    Type1,  
};

class SignedUrl
{
public:
	static std::shared_ptr<const SignedUrl> Load(SignedUrlType type, const ov::String &key, const ov::String &data);

	SignedUrlType			GetType() const;
    const ov::String&       GetUrl() const;
    const ov::String&       GetClientIP() const;
    const ov::String&       GetSessionID() const;
    uint64_t                GetTokenExpiredTime() const;
    uint64_t                GetStreamExpiredTime() const;
    bool                    IsAllowedClient(const ov::SocketAddress &address) const;
    bool                    IsTokenExpired() const;
    bool                    IsStreamExpired() const;

    bool ProcessType0(const ov::String &key, const ov::String &data);

    bool Encrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &plain_in, ov::Data &encrypted_out) const;
    bool Decrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &encrypted_in, ov::Data &plain_out) const;
    
private:
	SignedUrlType		_signed_type;

    ov::String  _key;
    ov::String  _full_string;

    ov::String  _url;
    ov::String  _client_ip;
    ov::String  _session_id;
    uint64_t    _token_expired_time;
    uint64_t    _stream_expired_time;
};
