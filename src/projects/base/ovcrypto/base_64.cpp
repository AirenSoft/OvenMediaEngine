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
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace ov
{
	ov::String Base64::Encode(const std::shared_ptr<const Data> &data)
	{
		if(data->GetLength() == 0L)
		{
			return "";
		}

		BIO *mem = BIO_new(BIO_s_mem());
		BIO *b64 = BIO_new(BIO_f_base64());

		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

		// b64의 결과를 mem이 받을 수 있게 함
		BIO_push(b64, mem);

		// base64 계산
		int result;
		int retry = 0;

		while(true)
		{
			result = BIO_write(b64, data->GetData(), static_cast<int>(data->GetLength()));

			if(result > 0)
			{
				OV_ASSERT2(result == data->GetLength());
				// 성공
				break;
			}

			if(BIO_should_retry(b64))
			{
				// 계속 진행

				retry++;

				if(retry == 10)
				{
					// 10회만 재시도 하고 종료
					OV_ASSERT2(false);
					break;
				}

				continue;
			}

			// 오류 발생
			break;
		}

		ov::String base64;

		if(result > 0)
		{
			// 성공
			BIO_flush(b64);

			// 결과값 받음
			char *buffer;
			long length = BIO_get_mem_data(mem, &buffer);

			base64 = ov::String(buffer, length);
		}
		else
		{
			// 오류 발생
			logtw("An error occurred while generate base64");
			OV_ASSERT2(false);
		}

		BIO_free_all(b64);

		return base64;
	}

	std::shared_ptr<Data> Base64::Decode(const ov::String &text)
	{
		if(text.IsEmpty())
		{
			return nullptr;
		}

		BIO *b64 = BIO_new(BIO_f_base64());
		BIO *mem = BIO_new_mem_buf(text.CStr(), static_cast<int>(text.GetLength()));

		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

		// b64의 결과를 mem이 받을 수 있게 함
		mem = BIO_push(b64, mem);

		// [base64 문자열 길이 / 4 * 3] + [null 문자] = (length / 4 * 3) + 1
		auto max_length = static_cast<int>((text.GetLength() / 4 * 3) + 1);

		auto data = std::make_shared<ov::Data>(max_length);
		data->SetLength(max_length);

		// base64 계산
		int result;
		int retry = 0;

		while(true)
		{
			result = BIO_read(b64, data->GetWritableData(), max_length);

			if(result > 0)
			{
				// 성공
				data->SetLength(result);

				break;
			}

			if(BIO_should_retry(b64))
			{
				// 계속 진행

				retry++;

				if(retry == 10)
				{
					// 10회만 재시도 하고 종료
					OV_ASSERT2(false);
					break;
				}

				continue;
			}

			// 오류 발생
			break;
		}

		ov::String base64;

		if(result <= 0)
		{
			// 오류 발생
			logtw("An error occurred while generate base64");
			OV_ASSERT2(false);
			data = nullptr;
		}

		BIO_free_all(b64);

		return data;
	}
}
