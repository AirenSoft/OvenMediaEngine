//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_response.h"
#include "http_client.h"
#include "http_private.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <base/ovsocket/ovsocket.h>

HttpResponse::HttpResponse(std::shared_ptr<HttpClient> client)
	: _http_client(std::move(client))
{
	OV_ASSERT2(_http_client != nullptr);
}

bool HttpResponse::SetHeader(const ov::String &key, const ov::String &value)
{
	if (_is_header_sent)
	{
		logtw("Cannot modify header: Header is sent");
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

	std::lock_guard<__decltype(_response_mutex)> lock(_response_mutex);

	_response_data_list.push_back(data);
	_response_data_size += data->GetLength();

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

bool HttpResponse::Response()
{
	std::lock_guard<__decltype(_response_mutex)> lock(_response_mutex);

	return SendHeaderIfNeeded() && SendResponse();
}

bool HttpResponse::SendHeaderIfNeeded()
{
	if (_is_header_sent)
	{
		// The headers are already sent
		return true;
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
	stream.Append(ov::String::FormatString("HTTP/1.1 %d %s\r\n", _status_code, _reason.CStr()).ToData(false));

	// RFC7230 - 3.2.  Header Fields
	std::for_each(_response_header.begin(), _response_header.end(), [&stream](const auto &pair) -> void {
		stream.Append(pair.first.ToData(false));
		stream.Append(": ", 2);
		stream.Append(pair.second.ToData(false));
		stream.Append("\r\n", 2);
	});

	stream.Append("\r\n", 2);

	if (_http_client->Send(response))
	{
		logtd("Header is sent");

		_is_header_sent = true;

		return true;
	}

	return false;
}

bool HttpResponse::SendResponse()
{
	bool sent = true;

	std::lock_guard<__decltype(_response_mutex)> lock(_response_mutex);

	logtd("Trying to send datas...");

	for (const auto &data : _response_data_list)
	{
		if (_chunked_transfer)
		{
			sent &= _http_client->SendChunkedData(data);
		}
		else
		{
			sent &= _http_client->Send(data);
		}
	}

	_response_data_list.clear();
	_response_data_size = 0ULL;

	logtd("All datas are sent...");

	return sent;
}

bool HttpResponse::Close()
{
	return _http_client->Close();
}
