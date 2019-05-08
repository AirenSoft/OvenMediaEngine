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

	// _response_header["Content-Length"] = ov::Converter::ToString(_response_data->GetCount());

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

// http header + body send
// first retry check and result check
ssize_t HttpResponse::Response(bool &is_retry)
{
    ssize_t sent = 0;

    if (_http_response_data == nullptr)
    {
        if(!MakeResponseData())
            return 0;
    }

    if(_tls == nullptr)
        sent = Send(_http_response_data->GetData(), _http_response_data->GetLength(), is_retry);
    else
        sent = Send(_http_tls_response_data->GetData(), _http_tls_response_data->GetLength(), is_retry);

    // sent data remove
    if (is_retry && sent != 0)
    {
       if(_tls == nullptr)
            _http_response_data = _http_response_data->Subdata(sent);
        else
            _http_tls_response_data = _http_tls_response_data->Subdata(sent);
    }

    return sent;
}

// http header + body data create
bool HttpResponse::MakeResponseData()
{
    _http_response_data = std::make_shared<ov::Data>();

    ov::ByteStream stream(_http_response_data.get());

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

    // tls data
    if(_tls != nullptr)
    {
        size_t written;

        // Data will be sent to the client
        //   ov::Tls::Write() -> HttpClient::TlsWriteToSave() -> HttpResponse::AppendTlsData()
        int result = _tls->Write(_http_response_data->GetData(), _http_response_data->GetLength(), &written);

        switch (result)
        {
            case SSL_ERROR_NONE:
                // data was sent in HttpClient::TlsWriteToSave();
                break;

            case SSL_ERROR_WANT_WRITE:
                // Need more data to encrypt
                break;

            default:
                logte("An error occurred while accept client: %s", _remote->ToString().CStr());
                return false;
        }
    }

    return true;
}

// HttpClient::TlsWriteToSave()  Call
bool HttpResponse::AppendTlsData(const void *data, size_t length)
{
    // tls handshake
    if(_http_response_data == nullptr)
    {
        if(_remote == nullptr)
            return false;

        return _remote->Send(data, length) == static_cast<ssize_t>(length);
    }

    if(_http_tls_response_data == nullptr)
        _http_tls_response_data = std::make_shared<ov::Data>();

    _http_tls_response_data->Append(data, length);

     return true;
}

ssize_t HttpResponse::Send(const void *data, size_t length, bool &is_retry)
{
    OV_ASSERT2(_remote != nullptr);

    if(_remote == nullptr)
    {
        return false;
    }

    // Send the plain data to the client
    return _remote->Send(data, length, is_retry);
}
