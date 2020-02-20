#include <openssl/evp.h>
#include <openssl/des.h>
#include <base/ovcrypto/base_64.h>
#include <base/ovlibrary/converter.h>

#include "signed_url.h"

// key = tivew_hlskey
// encrypted : fk2Wr7ZIPsT9TL/b/OtvekRHlskLoAf78M0gEr0jMX3T2hLrIqQVm6b2isFdVmS4rYombNEN7GiE1xsdctncRaHTmKtGUtgV1d2hyzDSyf+SSz7EAiye9GiExQsUk3yj1QEMdKMu+BaVp6l0omsNNIcwgjmfFx4Us6g6/Abdk4E=
// plain : https://tview-com/rtsp_live/12345/playlist.m3u8?rtspURI=rtsp://233.39.121.89:10906/69498/0,0.0.0.0,1581942526350,1581942526350
std::shared_ptr<const SignedUrl> SignedUrl::Load(SignedUrlType type, const ov::String &key, const ov::String &data)
{
    auto signed_url = std::make_shared<SignedUrl>();

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

uint64_t SignedUrl::GetNowMS() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

const ov::String& SignedUrl::GetUrl() const
{
    return _url;
}

const ov::String& SignedUrl::GetClientIP() const
{
    return _client_ip;
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

	if(GetNowMS() > _token_expired_time)
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

    if(GetNowMS() > _stream_expired_time)
	{
		return true;
	}

	return false;
}

bool SignedUrl::Parse(const ov::String &plain_string)
{
    auto items = plain_string.Split(",");

    if(items.size() != 4)
    {
        return false;
    }

    _full_string = plain_string;
    _url = items[0];
    _client_ip = items[1];
    _token_expired_time = ov::Converter::ToInt64(items[2]);
    _stream_expired_time = ov::Converter::ToInt64(items[3]);

    return true;
}

// Type0 : BASE64(DES/ECB/PKCS5 Padding)
bool SignedUrl::ProcessType0(const ov::String &key, const ov::String &data)
{
    auto const decoded_data = ov::Base64::Decode(data);

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
    decrypted_data.SetLength(decoded_data->GetLength()+1);
    int num_bytes_out = decrypted_data.GetLength();
    auto out = decrypted_data.GetWritableDataAs<uint8_t>();
    memset(out, 0, num_bytes_out);
    
    int total_out_length = 0;
    ret = EVP_DecryptUpdate(ctx, out, &num_bytes_out, (uint8_t *)decoded_data->GetWritableDataAs<uint8_t>(), decoded_data->GetLength());
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
        final_data = decrypted_data.Subdata(0, decrypted_data.GetLength() - padding_count - 1);
    }
    else
    {
        final_data = decrypted_data.Clone();
    }

    return Parse(final_data->ToString());
}

