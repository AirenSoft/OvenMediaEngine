//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_response.h"

#include <base/ovsocket/ovsocket.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "../http_private.h"

namespace http
{
	namespace svr
	{
		HttpResponse::HttpResponse(const std::shared_ptr<ov::ClientSocket> &client_socket)
			: _client_socket(client_socket)
		{
			OV_ASSERT2(_client_socket != nullptr);
		}

		HttpResponse::HttpResponse(const std::shared_ptr<HttpResponse> &http_response)
		{
			OV_ASSERT2(http_response != nullptr);

			_client_socket = http_response->_client_socket;
			_tls_data = http_response->_tls_data;
			_status_code = http_response->_status_code;
			_reason = http_response->_reason;
			_is_header_sent = http_response->_is_header_sent;
			_response_header = http_response->_response_header;
			_response_data_list = http_response->_response_data_list;
			_response_data_size = http_response->_response_data_size;
			_default_value = http_response->_default_value;
		}

		void HttpResponse::SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data)
		{
			_tls_data = tls_data;
		}

		std::shared_ptr<ov::ClientSocket> HttpResponse::GetRemote()
		{
			return _client_socket;
		}

		std::shared_ptr<const ov::ClientSocket> HttpResponse::GetRemote() const
		{
			return _client_socket;
		}

		std::shared_ptr<ov::TlsServerData> HttpResponse::GetTlsData()
		{
			return _tls_data;
		}

		StatusCode HttpResponse::GetStatusCode() const
		{
			return _status_code;
		}

		// Get Reason
		ov::String HttpResponse::GetReason()
		{
			return _reason;
		}

		// reason = default
		void HttpResponse::SetStatusCode(StatusCode status_code)
		{
			SetStatusCode(status_code, StringFromStatusCode(status_code));
		}

		// custom reason
		void HttpResponse::SetStatusCode(StatusCode status_code, const ov::String &reason)
		{
			_status_code = status_code;
			_reason = reason;
		}

		bool HttpResponse::AddHeader(const ov::String &key, const ov::String &value)
		{
			if (IsHeaderSent())
			{
				logtw("Cannot add header: Header is sent: %s", _client_socket->ToString().CStr());
				return false;
			}
			
			_response_header[key].push_back(value);

			return true;
		}

		bool HttpResponse::SetHeader(const ov::String &key, const ov::String &value)
		{
			if (IsHeaderSent())
			{
				logtw("Cannot set header: Header is sent: %s", _client_socket->ToString().CStr());
				return false;
			}
			
			_response_header[key] = {value};

			return true;
		}

		const std::vector<ov::String> &HttpResponse::GetHeader(const ov::String &key) const
		{
			auto item = _response_header.find(key);

			if (item == _response_header.end())
			{
				return _default_value;
			}

			return item->second;
		}

		bool HttpResponse::RemoveHeader(const ov::String &key)
		{
			if (IsHeaderSent())
			{
				logtw("Cannot remove header: Header is sent: %s", _client_socket->ToString().CStr());
				return false;
			}

			auto response_iterator = _response_header.find(key);

			if (response_iterator == _response_header.end())
			{
				return false;
			}

			_response_header.erase(response_iterator);

			return true;
		}

		bool HttpResponse::AppendData(const std::shared_ptr<const ov::Data> &data)
		{
			if (data == nullptr)
			{
				return false;
			}

			std::lock_guard<decltype(_response_mutex)> lock(_response_mutex);

			auto cloned_data = data->Clone();

			_response_data_list.push_back(cloned_data);
			_response_data_size += cloned_data->GetLength();

			return true;
		}

		bool HttpResponse::AppendString(const ov::String &string)
		{
			return AppendData(string.ToData(false));
		}

		bool HttpResponse::AppendFile(const ov::String &filename)
		{
			OV_ASSERT(false, "Not implemented");
			return false;
		}

		bool HttpResponse::IsHeaderSent() const
		{
			return _is_header_sent;
		}

		// Get Response Data Size
		size_t HttpResponse::GetResponseDataSize() const
		{
			return _response_data_size;
		}

		// Get Response Data List
		const std::vector<std::shared_ptr<const ov::Data>> &HttpResponse::GetResponseDataList() const
		{
			return _response_data_list;
		}

		// Get Response Header
		const std::unordered_map<ov::String, std::vector<ov::String>, ov::CaseInsensitiveHash, ov::CaseInsensitiveEqual> &HttpResponse::GetResponseHeaderList() const
		{
			return _response_header;
		}

		void HttpResponse::ResetResponseData()
		{
			_response_data_list.clear();
			_response_data_size = 0ULL;
		}

		uint32_t HttpResponse::Response()
		{
			std::lock_guard<decltype(_response_mutex)> lock(_response_mutex);
			
			uint32_t sent_size = 0;

			if (IsHeaderSent() == false)
			{
				auto sent_header_size = SendHeader();
				if (sent_header_size <= 0)
				{
					return -1;
				}

				sent_size += sent_header_size;
				if (sent_size > 0)
				{
					_is_header_sent = true;
				}
			}

			auto sent_data_size = SendPayload();
			if (sent_data_size <= 0)
			{
				return -1;
			}

			sent_size += sent_data_size;

			return sent_size;
		}	

		uint32_t HttpResponse::SendHeader()
		{
			return 0;
		}

		uint32_t HttpResponse::SendPayload()
		{
			return 0;
		}

		bool HttpResponse::Send(const void *data, size_t length)
		{
			return Send(std::make_shared<ov::Data>(data, length));
		}

		bool HttpResponse::Send(const std::shared_ptr<const ov::Data> &data)
		{
			if (data == nullptr)
			{
				OV_ASSERT2(data != nullptr);
				return false;
			}

			std::shared_ptr<const ov::Data> send_data;

			if (_tls_data == nullptr)
			{
				send_data = data->Clone();
			}
			else
			{
				if (_tls_data->Encrypt(data, &send_data) == false)
				{
					return false;
				}

				if ((send_data == nullptr) || send_data->IsEmpty())
				{
					// There is no data to send
					return true;
				}
			}

			return _client_socket->Send(send_data);
		}

		bool HttpResponse::Close()
		{
			OV_ASSERT2(_client_socket != nullptr);

			if (_client_socket == nullptr)
			{
				return false;
			}

			return _client_socket->CloseIfNeeded();
		}
	}  // namespace svr
}  // namespace http
