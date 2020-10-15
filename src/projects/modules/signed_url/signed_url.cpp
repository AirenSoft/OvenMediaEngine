#include <openssl/evp.h>
#include <openssl/des.h>
#include <base/ovcrypto/base_64.h>
#include <base/ovlibrary/converter.h>

#include "signed_url.h"

/* 
    [Test Code]

	SignedUrl sign;
	ov::String plain_token;
	ov::Data encrypted_data;

	auto mseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	plain_token.AppendFormat("%s,%s,%s,%llu,%llu", 
					"http://127.0.0.1:8090/rtsp_live/stream/playlist.m3u8?rtspURI=rtsp%3A%2F%2F203.67.18.25%3A554%2F0%2F", // url
					"0",	// ip
					"fdjksafj19283913921cdfjfas",	// session id
					mseconds + 10000,	// token expired ms
					0);	// stream expired ms

	sign.Encrypt_DES_ECB_PKCS5("tview_hlskey", *plain_token.ToData(false), encrypted_data);
	ov::String auth_token = ov::Base64::Encode(encrypted_data);
	
	logti("authtoken : %s", auth_token.CStr());

	auto urlencoded_authtoken = ov::Url::Encode(auth_token);

	logti("url encoded authtoken : %s", urlencoded_authtoken.CStr());

	ov::String url;
	url.AppendFormat("%s%s", "http://127.0.0.1:8090/rtsp_live/stream/playlist.m3u8?rtspURI=rtsp%3A%2F%2F203.67.18.25%3A554%2F0%2F&authtoken=", urlencoded_authtoken.CStr());

	logti("url : %s", url.CStr());
*/

std::shared_ptr<const SignedUrl> SignedUrl::Load(SignedUrlType type, const ov::String &key, const ov::String &data)
{
    auto signed_url = std::make_shared<SignedUrl>();
	signed_url->_signed_type = type;
	
    if(type == SignedUrlType::Type0)
    {
        if(signed_url->ProcessType0(key, data) == false)
        {
            return nullptr;
        }
    }
    else
    {
        return nullptr;
    }

    return signed_url;
}

const ov::String& SignedUrl::GetUrl() const
{
    return _url;
}

const ov::String& SignedUrl::GetClientIP() const
{
    return _client_ip;
}

const ov::String& SignedUrl::GetSessionID() const
{
    return _session_id;
}

uint64_t SignedUrl::GetTokenExpiredTime() const 
{
    return _token_expired_time;
}

uint64_t SignedUrl::GetStreamExpiredTime() const
{
    return _stream_expired_time;
}

bool SignedUrl::IsAllowedClient(const ov::SocketAddress &address) const
{
    if(_client_ip.IsEmpty() || _client_ip == "0.0.0.0" || _client_ip == "0" || _client_ip == address.GetIpAddress())
    {
        return true;
    }

    return false;
}

bool SignedUrl::IsTokenExpired() const
{
    if(_token_expired_time == 0)
    {
        return false;
    }

	if(ov::Clock::NowMS() > _token_expired_time)
	{
		return true;
	}

	return false;
}

bool SignedUrl::IsStreamExpired() const
{
    if(_stream_expired_time == 0)
    {
        return false;
    }

    if(ov::Clock::NowMS() > _stream_expired_time)
	{
		return true;
	}

	return false;
}

// Type0 : BASE64(DES/ECB/PKCS5 Padding)
// Struct : <URL - https://~~?rtspURI=~>,<Client IP>,<Session ID>,<Token Expired>,<Stream Expired>
//      Client IP : empty, 0 or 0.0.0.0 will accept all addresses
//      Session ID : UUID
//      Token Expired Time : milliseconds from epoch, empty or 0 won't be expired
//      Stream Expired Time : milliseconds from epoch, empty or 0 won't be expired
bool SignedUrl::ProcessType0(const ov::String &key, const ov::String &data)
{
    ov::Data final_data;
    auto const decoded_data = ov::Base64::Decode(data);
    if(decoded_data == nullptr)
    {
        return false;
    }

    if(!Decrypt_DES_ECB_PKCS5(key, *decoded_data, final_data))
    {
        return false;
    }

    auto plain_string = final_data.ToString();
    auto items = plain_string.Split(",");
    if(items.size() != 5)
    {
        return false;
    }

    _full_string = plain_string;
    _url = items[0];
    _client_ip = items[1];
    _session_id = items[2];
    _token_expired_time = ov::Converter::ToInt64(items[3]);
    _stream_expired_time = ov::Converter::ToInt64(items[4]);

    return true;
}

bool SignedUrl::Encrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &plain_in, ov::Data &encrypted_out) const
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(ctx == nullptr)
    {
        return false;
    }

    int ret; 

    ret = EVP_CIPHER_CTX_init(ctx);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ret = EVP_EncryptInit_ex(ctx, EVP_des_ecb(), NULL, (uint8_t *)key.CStr(), NULL);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    uint32_t plain_data_len = plain_in.GetLength();
    uint32_t encrypted_data_len = plain_in.GetLength() + (8 - (plain_in.GetLength() % 8 == 0 ? 0 : plain_in.GetLength() % 8));
    encrypted_out.SetLength(encrypted_data_len);

    auto in = plain_in.GetWritableDataAs<uint8_t>();
    auto out = encrypted_out.GetWritableDataAs<uint8_t>();
    memset(out, 0, encrypted_out.GetLength());

    uint32_t total_out_length = 0;
    int num_bytes_out = 0;

    ret = EVP_EncryptUpdate(ctx, out, &num_bytes_out, in, plain_data_len);
    if(ret != 1)
    {    
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    total_out_length += num_bytes_out;

    ret = EVP_EncryptFinal_ex(ctx, out + total_out_length, &num_bytes_out);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    total_out_length += num_bytes_out;
    EVP_CIPHER_CTX_free(ctx);

    return true;
}

bool SignedUrl::Decrypt_DES_ECB_PKCS5(const ov::String &key, ov::Data &encrypted_in, ov::Data &plain_out) const
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(ctx == nullptr)
    {
        return false;
    }

    int ret; 

    ret = EVP_CIPHER_CTX_init(ctx);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ret = EVP_DecryptInit_ex(ctx, EVP_des_ecb(), NULL, (uint8_t *)key.CStr(), NULL);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ret = EVP_CIPHER_CTX_set_padding(ctx, 0);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ov::Data decrypted_data;
    decrypted_data.SetLength(encrypted_in.GetLength()+1);
    int num_bytes_out = decrypted_data.GetLength();
    auto out = decrypted_data.GetWritableDataAs<uint8_t>();
    memset(out, 0, num_bytes_out);
    
    int total_out_length = 0;
    ret = EVP_DecryptUpdate(ctx, out, &num_bytes_out, (uint8_t *)encrypted_in.GetWritableDataAs<uint8_t>(), encrypted_in.GetLength());
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    total_out_length += num_bytes_out;
    ret = EVP_DecryptFinal_ex(ctx, out + num_bytes_out, &num_bytes_out);
    if(ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    total_out_length += num_bytes_out;
    EVP_CIPHER_CTX_free(ctx);

    // There are paddings in decrypted text
    std::shared_ptr<ov::Data> final_data;
    if(out[total_out_length-1] >= 1 && out[total_out_length-1] <= 8)
    {
        int padding_count = out[total_out_length-1];
        plain_out = *decrypted_data.Subdata(0, decrypted_data.GetLength() - padding_count - 1);
    }
    else
    {
        plain_out = decrypted_data;
    }

    return true;
}
