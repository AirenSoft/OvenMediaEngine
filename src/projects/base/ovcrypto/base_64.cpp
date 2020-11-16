//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "base_64.h"
#include "ovcrypto_private.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

namespace ov
{
	ov::String Base64::Encode(const Data &data, bool url)
	{
		if (data.GetLength() == 0L)
		{
			return "";
		}

		BIO *mem = BIO_new(BIO_s_mem());
		BIO *b64 = BIO_new(BIO_f_base64());

		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		mem = BIO_push(b64, mem);

		int result;
		int retry = 0;

		while (true)
		{
			result = BIO_write(b64, data.GetData(), static_cast<int>(data.GetLength()));

			if (result > 0)
			{
				OV_ASSERT2(static_cast<size_t>(result) == data.GetLength());
				break;
			}

			if (BIO_should_retry(b64))
			{
				retry++;

				if (retry == 10)
				{
					logte("Tried to write data %d times but failed", retry);

					OV_ASSERT2(false);
					break;
				}

				continue;
			}

			break;
		}

		ov::String base64;

		if (result > 0)
		{
			(void)BIO_flush(b64);

			char *buffer;
			long length = BIO_get_mem_data(mem, &buffer);

			base64 = ov::String(buffer, length);
		}
		else
		{
			logtw("An error occurred while generate base64");
			OV_ASSERT2(false);
		}

		BIO_free_all(b64);

		if(url == true && !base64.IsEmpty())
		{
			// TODO(Getroot): Need to optimize for performance
			base64 = base64.Replace("+", "-");
			base64 = base64.Replace("/", "_");

			// Remove '=' characters
			if(base64.GetBuffer()[base64.GetLength()-1] == '=')
			{
				if(base64.GetBuffer()[base64.GetLength()-2] == '=')
				{
					base64.SetLength(base64.GetLength() - 2);
				}
				else
				{
					base64.SetLength(base64.GetLength() - 1);
				}
			}
		}

		return base64;
	}

	ov::String Base64::Encode(const std::shared_ptr<const Data> &data, bool url)
	{
		return Encode(*data, url);
	}

	std::shared_ptr<Data> Base64::Decode(const ov::String &text, bool url)
	{
		if (text.IsEmpty())
		{
			return nullptr;
		}

		ov::String source = text;

		// TODO(Getroot): Need to optimize for performance
		if(url == true)
		{
			// replace any charaters
			source = source.Replace("-", "+");
			source = source.Replace("_", "/");

			uint8_t pad_count = 4-(source.GetLength() % 4);
			if(pad_count == 4)
			{
				pad_count = 0;
			}

			for(uint8_t i = 0; i<pad_count; i++)
			{	
				source.Append("=");
			}
		}

		BIO *b64 = BIO_new(BIO_f_base64());
		BIO *mem = BIO_new_mem_buf(source.CStr(), static_cast<int>(source.GetLength()));

		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

		mem = BIO_push(b64, mem);

		// [base64 string length / 4 * 3] + [null character] = (length / 4 * 3) + 1
		auto max_length = static_cast<int>((source.GetLength()) + 1);

		auto data = std::make_shared<ov::Data>(max_length);
		data->SetLength(max_length);

		// base64 calculate
		int result;
		int retry = 0;

		while (true)
		{
			result = BIO_read(mem, data->GetWritableData(), max_length);
			if (result > 0)
			{
				data->SetLength(result);
				break;
			}

			if (BIO_should_retry(mem))
			{
				retry++;

				if (retry == 10)
				{
					OV_ASSERT2(false);
					break;
				}

				continue;
			}

			break;
		}

		if (result <= 0)
		{
			logtw("An error occurred while generate base64");
			OV_ASSERT2(false);
			data = nullptr;
		}

		BIO_free_all(mem);

		return data;
	}
}  // namespace ov
