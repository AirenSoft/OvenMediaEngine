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

#include "http_client.h"
#include "http_private.h"

HttpResponse::HttpResponse(const std::shared_ptr<ov::ClientSocket> &client_socket)
	: _client_socket(client_socket)
{
	OV_ASSERT2(_client_socket != nullptr);
}

void HttpResponse::SetTlsData(const std::shared_ptr<ov::TlsData> &tls_data)
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

std::shared_ptr<ov::TlsData> HttpResponse::GetTlsData()
{
	return _tls_data;
}

bool HttpResponse::SetHeader(const ov::String &key, const ov::String &value)
{
	if (_is_header_sent)
	{
		logtw("Cannot modify header: Header is sent: %s", _client_socket->ToString().CStr());
		return false;
	}

	_response_header[key] = value;

	return true;
}

const ov::String &HttpResponse::GetHeader(const ov::String &key)
{
	auto item = _response_header.find(key);

	if (item == _response_header.end())
	{
		return _default_value;
	}

	return item->second;
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
	// TODO: ov::Data에 파일에서 읽는 기능이 추가되면 그 때 구현
	OV_ASSERT(false, "Not implemented");

	return false;
}

uint32_t HttpResponse::Response()
{
	std::lock_guard<decltype(_response_mutex)> lock(_response_mutex);

	return SendHeaderIfNeeded() + SendResponse();
}

uint32_t HttpResponse::SendHeaderIfNeeded()
{
	if (_is_header_sent)
	{
		// The headers are already sent
		return 0;
	}

	std::shared_ptr<ov::Data> response = std::make_shared<ov::Data>();
	ov::ByteStream stream(response.get());

	if (_chunked_transfer == false)
	{
		// Calculate the content length
		SetHeader("Content-Length", ov::Converter::ToString(_response_data_size));
	}

	// RFC7230 - 3.1.2.  Status Line
	// status-line = HTTP-version SP status-code SP reason-phrase CRLF
	// TODO(dimiden): Replace this HTTP version with the version that received from the request
	stream.Append(ov::String::FormatString("HTTP/%s %d %s\r\n", _http_version.CStr(), _status_code, _reason.CStr()).ToData(false));

	// RFC7230 - 3.2.  Header Fields
	std::for_each(_response_header.begin(), _response_header.end(), [&stream](const auto &pair) -> void {
		stream.Append(pair.first.ToData(false));
		stream.Append(": ", 2);
		stream.Append(pair.second.ToData(false));
		stream.Append("\r\n", 2);
	});

	stream.Append("\r\n", 2);

	if (Send(response))
	{
		logtd("Header is sent:\n%s", response->Dump(response->GetLength()).CStr());

		_is_header_sent = true;

		return response->GetLength();
	}

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

bool HttpResponse::SendChunkedData(const void *data, size_t length)
{
	return SendChunkedData(std::make_shared<ov::Data>(data, length));
}

bool HttpResponse::SendChunkedData(const std::shared_ptr<const ov::Data> &data)
{
	if ((data == nullptr) || data->IsEmpty())
	{
		// Send a empty chunk
		return Send("0\r\n\r\n", 5);
	}

	bool result =
		// Send the chunk header
		Send(ov::String::FormatString("%x\r\n", data->GetLength()).ToData(false)) &&
		// Send the chunk payload
		Send(data) &&
		// Send a last data of chunk
		Send("\r\n", 2);

	return result;
}

uint32_t HttpResponse::SendResponse()
{
	bool sent = true;

	std::lock_guard<decltype(_response_mutex)> lock(_response_mutex);

	logtd("Trying to send datas...");

	uint32_t sent_bytes = 0;
	for (const auto &data : _response_data_list)
	{
		if (_chunked_transfer)
		{
			sent &= SendChunkedData(data);
			if (sent == true)
			{
				sent_bytes += data->GetLength();
			}
		}
		else
		{
			sent &= Send(data);
			if (sent == true)
			{
				sent_bytes += data->GetLength();
			}
		}
	}

	_response_data_list.clear();
	_response_data_size = 0ULL;

	logtd("All datas are sent...");

	return sent_bytes;
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
