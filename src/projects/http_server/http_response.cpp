//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_response.h"
#include "http_request.h"
#include "http_private.h"

#include <utility>
#include <memory>
#include <algorithm>

#include <base/ovsocket/ovsocket.h>

HttpResponse::HttpResponse(HttpRequest *request, std::shared_ptr<ov::ClientSocket> remote)
	: _request(request),
	  _remote(std::move(remote))
{
	OV_ASSERT2(_remote != nullptr);
}

bool HttpResponse::Response()
{
	return SendHeaderIfNeeded() && SendResponse();
}

bool HttpResponse::AppendData(const std::shared_ptr<const ov::Data> &data)
{
	if(data == nullptr)
	{
		return false;
	}

	_response_data_list.push_back(data);

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

bool HttpResponse::SetHeader(const ov::String &key, const ov::String &value)
{
	if(_is_header_sent)
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

	if(item == _response_header.end())
	{
		return _default_value;
	}

	return item->second;
}

bool HttpResponse::Send(const void *data, size_t length)
{
	OV_ASSERT2(_remote != nullptr);

	if(_remote == nullptr)
	{
		return false;
	}

	if(_tls != nullptr)
	{
		size_t written;

		// Data will be sent to the client

		// * Data flow:
		//   1. ov::Tls::Write() ->
		//   2. HttpClient::TlsWrite() ->
		//   3. ClientSocket::Send() // using HttpResponse::_remote
		int result = _tls->Write(data, length, &written);

		switch(result)
		{
			case SSL_ERROR_NONE:
				// data was sent in HttpClient::TlsWrite();
				break;

			case SSL_ERROR_WANT_WRITE:
				// Need more data to encrypt
				break;

			default:
				logte("An error occurred while accept client: %s", _remote->ToString().CStr());
				return false;
		}

		return true;
	}
	else
	{
		// Send the plain data to the client
		return _remote->Send(data, length) == static_cast<ssize_t>(length);
	}
}

bool HttpResponse::Send(const std::shared_ptr<const ov::Data> &data)
{
	if(data->GetLength() == 0)
	{
		return true;
	}

	return Send(data->GetData(), data->GetLength());
}

bool HttpResponse::SendHeaderIfNeeded()
{
	if(_is_header_sent)
	{
		// 헤더를 보낸 상태
		return true;
	}

	if(_remote == nullptr)
	{
		return false;
	}

	std::shared_ptr<ov::Data> response = std::make_shared<ov::Data>();
	ov::ByteStream stream(response.get());

	// RFC7230 - 3.1.2.  Status Line
	// status-line = HTTP-version SP status-code SP reason-phrase CRLF
	// TODO: HTTP version을 request에서 받은 것으로 대체
	stream.Append(ov::String::FormatString("HTTP/1.1 %d %s\r\n", _status_code, _reason.CStr()).ToData(false));

	// RFC7230 - 3.2.  Header Fields
	std::for_each(_response_header.begin(), _response_header.end(), [&stream](const auto &pair) -> void
	{
		stream.Append(pair.first.ToData(false));
		stream.Append(": ", 2);
		stream.Append(pair.second.ToData(false));
		stream.Append("\r\n", 2);
	});

	stream.Append("\r\n", 2);

	if(Send(response))
	{
		_is_header_sent = true;

		return true;
	}

	return false;
}

bool HttpResponse::SendResponse()
{
	bool sent = true;

	for(const auto &data : _response_data_list)
	{
		sent &= Send(data);
	}

	_response_data_list.clear();

	return sent;
}



// data : http header + body send
// post send
bool HttpResponse::PostResponse()
{
	// data empty or already sended
	if(_response_data_list.empty() && _response_header.empty())
	{
		return true;
	}

    bool result = false;
	auto response_data = MakeResponseData();

	if(response_data == nullptr || response_data->GetLength() == 0)
	{
		logte("Post response data fail : %s", _remote->ToString().CStr());
		return false;
	}

	result = PostSend(response_data->GetData(), response_data->GetLength());

	// chunked transfer init
	_chunked_transfer = false;

	return result;
}

// http header + body data create
// - supported : chunked-transfer
std::shared_ptr<ov::Data> HttpResponse::MakeResponseData()
{
    auto response_data = std::make_shared<ov::Data>();

    ov::ByteStream stream(response_data.get());

    // Content-Length
    uint32_t content_length = 0;
	for(const auto &data : _response_data_list)
	{
		content_length += data->GetLength();
	}

	// chunked-transfer supported
	if(!_chunked_transfer)
		_response_header["Content-Length"] = ov::Converter::ToString(content_length);

    // header
    stream.Append(ov::String::FormatString("HTTP/1.1 %d %s\r\n", _status_code, _reason.CStr()).ToData(false));
    std::for_each(_response_header.begin(), _response_header.end(), [&stream](const auto &pair) -> void
    {
        stream.Append(pair.first.ToData(false));
        stream.Append(": ", 2);
        stream.Append(pair.second.ToData(false));
        stream.Append("\r\n", 2);
    });
    stream.Append("\r\n", 2);

    // body
    for(const auto &data : _response_data_list)
    {
        stream.Append(data->GetData(), data->GetLength());
    }

	_response_data_list.clear();
	_response_header.clear();

    return response_data;
}

bool HttpResponse::PostChunkedDataResponse(const std::shared_ptr<const ov::Data> &data)
{
	if(data == nullptr || data->GetLength() <= 0)
	{
		logtw("Post chunked data is empty : %s", _remote->ToString().CStr());
		return false;
	}

	return PostSend(data->GetData(), data->GetLength());
}

bool HttpResponse::PostChunkedEndResponse()
{
	// chunked transfer init
	_chunked_transfer = false;
	return PostSend("0\r\n\r\n", 5);
}

bool HttpResponse::PostSend(const void *data, size_t length)
{
	OV_ASSERT2(_remote != nullptr);

	if(_remote == nullptr)
	{
		return false;
	}

	if(_tls == nullptr)
	{
		return _remote->PostSend(data, length);
	}

	size_t written;
	bool result = false;

	// ov::Tls::Write() -> HttpClient::TlsWrite() ->  PostSend() or Send()
	int tls_result = _tls->Write(data, length, &written);

	switch(tls_result)
	{
		case SSL_ERROR_NONE:
			// data was sent in HttpClient::TlsWrite();
			result = true;
			break;

		case SSL_ERROR_WANT_WRITE:
			// Need more data to encrypt
			result = true;
			break;

		default:
			logte("An error occurred while accept client: %s", _remote->ToString().CStr());
			result = false;
	}

	return result;
}