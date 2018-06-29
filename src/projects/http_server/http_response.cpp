//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_response.h"
#include "http_private.h"

#include <memory>
#include <algorithm>

#include <base/ovsocket/ovsocket.h>


HttpResponse::HttpResponse(ov::ClientSocket *remote)
	: _status_code(HttpStatusCode::OK),
	  _reason(StringFromHttpStatusCode(HttpStatusCode::OK)),

	  _is_header_sent(false),

	  _remote(remote)
{
	OV_ASSERT2(_remote != nullptr);
}

HttpResponse::~HttpResponse()
{
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

	ov::String response_message;

	// RFC7230 - 3.1.2.  Status Line
	// status-line = HTTP-version SP status-code SP reason-phrase CRLF
	// TODO: HTTP version을 request에서 받은 것으로 대체
	response_message.Format("HTTP/1.1 %d %s\r\n", _status_code, _reason.CStr());

	// RFC7230 - 3.2.  Header Fields
	std::for_each(_response_header.begin(), _response_header.end(), [&response_message](const auto &pair) -> void
	{
		response_message.AppendFormat("%s: %s\r\n", pair.first.CStr(), pair.second.CStr());
	});

	response_message.AppendFormat("\r\n");

	_remote->Send(response_message);

	_is_header_sent = true;

	return true;
}

bool HttpResponse::SendResponse()
{
	bool sent = true;

	for(const auto &data : _response_data_list)
	{
		sent &= (_remote->Send(data) == data->GetLength());
	}

	_response_data_list.clear();

	return sent;
}
